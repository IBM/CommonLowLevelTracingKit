// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "commands/interface.hpp"
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace rj = rapidjson;

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;

struct TracepipeEntry {
	std::string tracebuffer;
	uint32_t pid;
	uint32_t tid;
	std::string message;
	std::string file;
	size_t line;
};

void parse_json(const rj::Value &j, TracepipeEntry &p)
{
	if (j.HasMember("pid") && j["pid"].IsUint()) {
		p.pid = j["pid"].GetUint();
	} else {
		p.pid = 0;
	}

	if (j.HasMember("tid") && j["tid"].IsUint()) {
		p.tid = j["tid"].GetUint();
	} else {
		p.tid = 0;
	}

	// Required field
	if (j.HasMember("message") && j["message"].IsString()) {
		p.message = j["message"].GetString();
	} else {
		throw std::runtime_error("Missing required 'message' field");
	}

	if (j.HasMember("file") && j["file"].IsString()) {
		p.file = j["file"].GetString();
	} else {
		p.file = "";
	}

	if (j.HasMember("line") && j["line"].IsUint64()) {
		p.line = j["line"].GetUint64();
	} else {
		p.line = 0;
	}
}

class Command
{
  public:
	static void AddCommand(CLI::App &parent)
	{
		auto cmd = std::make_shared<Command>();

		cmd->sub = parent.add_subcommand("tracepipe", "Pipe tracepoints from stdin or file");
		cmd->sub->alias("tpipe");
		cmd->sub->description(
			"Read lines from stdin or a file and write each as a tracepoint to a tracebuffer.\n"
			"Supports plain text (one line = one tracepoint) or JSON format.\n"
			"Useful for bulk trace injection from logs, scripts, or piped program output.\n\n"
			"JSON format: {\"message\": \"required\", \"pid\": 0, \"tid\": 0, \"file\": \"\", "
			"\"line\": 0}\n"
			"Only 'message' is required; other fields default to 0 or empty string.");

		cmd->sub
			->add_option("buffer,-b,--buffer", cmd->buffer_name,
						 "Target tracebuffer name.\n"
						 "The tracebuffer will be created if it does not exist")
			->check(validator::TracebufferName{})
			->required()
			->type_name("NAME");

		cmd->sub
			->add_option("-s,--size", cmd->buffer_size,
						 "Ring buffer size in bytes if creating a new tracebuffer.\n"
						 "One basic tracepoint is approximately 32 bytes.\n"
						 "Supports size suffixes: K, M, G (e.g., 512K, 1M)")
			->capture_default_str()
			->transform(CLI::AsSizeValue{false})
			->type_name("SIZE");

		cmd->sub->add_flag("-j,--json", cmd->try_json,
						   "Expect JSON input (one JSON object per line).\n"
						   "Keys: pid, tid, message (required), file, line.\n"
						   "If JSON parsing fails, falls back to plain text mode");

		cmd->sub
			->add_option("input_file", cmd->input_file,
						 "Input file path. Reads from stdin if not specified (use - for stdin)")
			->type_name("FILE");

		cmd->sub->callback(std::bind(&Command::run, std::move(cmd)));
	}

  protected:
	CLI::App *sub;
	std::string buffer_name{};
	uint32_t buffer_size{512000};
	std::string input_file;
	bool try_json{false};

	static void handle_line(Command &cmd, std::istream &input)
	{
		std::string line;
		size_t line_count = 0;
		while (!is_interrupted() && std::getline(input, line)) {
			++line_count;

			if (cmd.try_json) {
				try {
					rj::Document doc;
					doc.Parse(line.c_str());

					if (doc.HasParseError()) {
						log_error("JSON parse error: ", rj::GetParseError_En(doc.GetParseError()),
								  " at offset ", doc.GetErrorOffset());
						// continue with backup case
					} else {
						TracepipeEntry entry;
						parse_json(doc, entry);
						clltk_dynamic_tracepoint_execution(
							cmd.buffer_name.c_str(), entry.file.c_str(), entry.line, entry.pid,
							entry.tid, "%s", entry.message.c_str());
						continue;
					}
				} catch (const std::exception &e) {
					log_error("Invalid JSON: ", e.what());
					// continue with backup case
				}
			}

			// default and backup case
			clltk_dynamic_tracepoint_execution(cmd.buffer_name.c_str(), "", 0, 0, 0, "%s",
											   line.c_str());
		}

		if (is_interrupted()) {
			log_info("Interrupted after ", line_count, " lines");
		} else {
			log_verbose("Processed ", line_count, " lines");
		}
	}

	static void run(std::shared_ptr<Command> cmd)
	{
		CommonLowLevelTracingKit::cmd::interface::sync_path_to_library();
		clltk_dynamic_tracebuffer_creation(cmd->buffer_name.c_str(), cmd->buffer_size);

		if (cmd->input_file.empty() || cmd->input_file == "-") {
			handle_line(*cmd, std::cin);
		} else {
			std::ifstream infile(cmd->input_file);
			if (!infile.is_open()) {
				throw CLI::RuntimeError("Failed to open input file: " + cmd->input_file, 1);
			}
			handle_line(*cmd, infile);
			infile.close();
		}
	}
};

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	Command::AddCommand(app);
}
COMMAND_INIT(init_function);
