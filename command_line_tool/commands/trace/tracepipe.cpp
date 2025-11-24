// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "commands/interface.hpp"
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;

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

void from_json(const json &j, TracepipeEntry &p)
{
	p.pid = j.value("pid", 0);
	p.tid = j.value("tid", 0);
	p.message = j.at("message"); // only required value
	p.file = j.value("file", "");
	p.line = j.value("line", 0);
}

class Command
{
  public:
	static void AddCommand(CLI::App &parent)
	{
		auto cmd = std::make_shared<Command>();

		// trace command
		static const std::string description =
			"Pipe tracepoints from stdin, file or file-like to tracebuffer.";
		cmd->sub = parent.add_subcommand("tracepipe", description);

		cmd->sub->add_option("tracebuffer-name,--tracebuffer-name", cmd->tracebuffer_name, "")
			->check(validator::TracebufferName{})
			->required();

		cmd->sub
			->add_option("tracebuffer-size,--tracebuffer-size", cmd->tracebuffer_size,
						 "The size of the ringbuffer in this tracebuffer in bytes. (one tracepoint "
						 "entry, without any arguments will be ~ 32bytes)")
			->capture_default_str()
			->transform(CLI::AsSizeValue{false});

		cmd->sub
			->add_flag(
				"--json", cmd->try_json,
				"Expect input as json object per line with keys: pid, tid, message, file, line")
			->capture_default_str();

		cmd->sub->add_option("input_file", cmd->input_file, "")->check(CLI::ExistingFile);

		cmd->sub->callback(std::bind(&Command::run, std::move(cmd)));
	}

  protected:
	CLI::App *sub;
	std::string tracebuffer_name{"CommonLowLevelTracingKit-cmd-tracepipe-output"};
	uint32_t tracebuffer_size{512000};
	std::string input_file;
	bool try_json{false};

	static void handle_line(Command &cmd, std::istream &input)
	{
		std::string line;
		while (std::getline(input, line)) {

			if (cmd.try_json) {
				try {

					auto data = json::parse(line);
					auto entry = data.get<TracepipeEntry>();
					clltk_dynamic_tracepoint_execution(cmd.tracebuffer_name.c_str(),
													   entry.file.c_str(), entry.line, entry.pid,
													   entry.tid, "%s", entry.message.c_str());
					continue;
				} catch (const json::exception &e) {
					std::cerr << "no valid json: " << e.what() << std::endl;
					// continue with backup case
				}
			}

			// default and backup case
			clltk_dynamic_tracepoint_execution(cmd.tracebuffer_name.c_str(), "", 0, 0, 0, "%s",
											   line.c_str());
		}
	}

	static void run(std::shared_ptr<Command> cmd)
	{
		clltk_dynamic_tracebuffer_creation(cmd->tracebuffer_name.c_str(), cmd->tracebuffer_size);

		if (cmd->input_file.empty()) {
			handle_line(*cmd, std::cin);
		} else {
			std::ifstream infile(cmd->input_file);
			handle_line(*cmd, infile);
			infile.close();
		}
	}
};

static int init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	Command::AddCommand(app);
	return 0;
}

static const int dummy = init_function();
