// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <boost/regex.hpp>
#include <chrono>
#include <iomanip>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sstream>
#include <string>

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "commands/filter.hpp"
#include "commands/interface.hpp"

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;
using TraceBufferInfo = CommonLowLevelTracingKit::decoder::TraceBufferInfo;
using TraceBufferInfoCollection = CommonLowLevelTracingKit::decoder::TraceBufferInfoCollection;
using SourceType = CommonLowLevelTracingKit::decoder::SourceType;

static std::string source_type_to_string(SourceType type)
{
	switch (type) {
	case SourceType::Userspace:
		return "user";
	case SourceType::Kernel:
		return "kernel";
	case SourceType::TTY:
		return "tty";
	case SourceType::Unknown:
	default:
		return "?";
	}
}

static std::string format_time(const std::filesystem::file_time_type &time)
{
	// Convert file_time_type to system_clock::time_point
	auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
		time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
	auto tt = std::chrono::system_clock::to_time_t(sctp);
	std::tm tm{};
	localtime_r(&tt, &tm);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

static std::string format_time_iso(const std::filesystem::file_time_type &time)
{
	auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
		time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
	auto tt = std::chrono::system_clock::to_time_t(sctp);
	std::tm tm{};
	gmtime_r(&tt, &tm);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
	return oss.str();
}

static void print_table_header(FILE *f, size_t name_width)
{
	fprintf(f, "%-*s %-6s %12s %12s %5s %10s %10s %10s %8s %-19s %s\n",
			static_cast<int>(name_width), "NAME", "SOURCE", "CAPACITY", "USED", "FILL", "ENTRIES",
			"PENDING", "DROPPED", "WRAPPED", "MODIFIED", "PATH");
}

static void print_table_row(FILE *f, const TraceBufferInfo &info, size_t name_width)
{
	if (info.valid()) {
		fprintf(f, "%-*s %-6s %12lu %12lu %4.0f%% %10lu %10lu %10lu %8lu %-19s %s\n",
				static_cast<int>(name_width), info.name.c_str(),
				source_type_to_string(info.source_type).c_str(), info.capacity, info.used,
				info.fill_percent, info.entries, info.pending, info.dropped, info.wrapped,
				format_time(info.modified).c_str(), info.path.string().c_str());
	} else {
		fprintf(f, "%-*s %-6s %12s %12s %5s %10s %10s %10s %8s %-19s %s\n",
				static_cast<int>(name_width), info.name.c_str(), "?", "?", "?", "?", "?", "?", "?",
				"?", format_time(info.modified).c_str(), info.path.string().c_str());
	}
}

static void print_json_output(FILE *f, const TraceBufferInfoCollection &infos)
{
	rapidjson::Document doc;
	doc.SetArray();
	auto &allocator = doc.GetAllocator();

	for (const auto &info : infos) {
		rapidjson::Value obj;
		obj.SetObject();

		rapidjson::Value name;
		name.SetString(info.name.c_str(), allocator);
		obj.AddMember("name", name, allocator);

		rapidjson::Value path;
		path.SetString(info.path.string().c_str(), allocator);
		obj.AddMember("path", path, allocator);

		if (info.valid()) {
			rapidjson::Value source_type;
			source_type.SetString(source_type_to_string(info.source_type).c_str(), allocator);
			obj.AddMember("source_type", source_type, allocator);

			obj.AddMember("capacity", rapidjson::Value(info.capacity), allocator);
			obj.AddMember("used", rapidjson::Value(info.used), allocator);
			obj.AddMember("available", rapidjson::Value(info.available), allocator);
			obj.AddMember("fill_percent", rapidjson::Value(info.fill_percent), allocator);
			obj.AddMember("entries", rapidjson::Value(info.entries), allocator);
			obj.AddMember("pending", rapidjson::Value(info.pending), allocator);
			obj.AddMember("dropped", rapidjson::Value(info.dropped), allocator);
			obj.AddMember("wrapped", rapidjson::Value(info.wrapped), allocator);

			rapidjson::Value modified;
			modified.SetString(format_time_iso(info.modified).c_str(), allocator);
			obj.AddMember("modified", modified, allocator);
		} else {
			rapidjson::Value error;
			error.SetString(info.error.value_or("unknown error").c_str(), allocator);
			obj.AddMember("error", error, allocator);
		}

		doc.PushBack(obj, allocator);
	}

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	fprintf(f, "%s\n", buffer.GetString());
}

static void add_list_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("list", "List tracebuffers with statistics");
	command->alias("ls");
	command->description("List all tracebuffers in a directory with detailed statistics.\n"
						 "Shows buffer capacity, usage, entry counts, and other metadata.\n"
						 "If no path is specified, uses CLLTK_TRACING_PATH or current directory.");

	static std::string input_path{};
	command
		->add_option("path", input_path,
					 "Path to search for tracebuffers\n"
					 "(default: CLLTK_TRACING_PATH or current directory)")
		->type_name("PATH");

	static bool recursive = false;
	command->add_flag("-r,--recursive", recursive, "Recurse into subdirectories");

	static std::string filter_str =
		CommonLowLevelTracingKit::cmd::interface::default_filter_pattern;
	CommonLowLevelTracingKit::cmd::interface::add_filter_option(command, filter_str);

	static bool json_output = false;
	command->add_flag("-j,--json", json_output, "Output as JSON");

	command->callback([&]() {
		// Resolve input path: use provided path, or fall back to tracing path
		std::string resolved_input = input_path.empty() ? get_tracing_path().string() : input_path;

		// Build filter function from regex
		const boost::regex filter_regex{filter_str};
		auto filter_func = [&](const std::string &name) -> bool {
			return CommonLowLevelTracingKit::cmd::interface::match_tracebuffer_filter(name,
																					  filter_regex);
		};

		// Get tracebuffer list
		auto infos = CommonLowLevelTracingKit::decoder::listTraceBuffers(resolved_input, recursive,
																		 filter_func);

		if (json_output) {
			print_json_output(stdout, infos);
		} else {
			if (infos.empty()) {
				log_info("No tracebuffers found in ", resolved_input);
			} else {
				// Calculate max name width for alignment
				size_t name_width = 4; // minimum width for "NAME" header
				for (const auto &info : infos) {
					name_width = std::max(name_width, info.name.size());
				}

				print_table_header(stdout, name_width);
				for (const auto &info : infos) {
					print_table_row(stdout, info, name_width);
				}
			}
		}
		return 0;
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_list_command(app);
}
COMMAND_INIT(init_function);
