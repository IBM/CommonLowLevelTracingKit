// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <boost/regex.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>

#include "CommonLowLevelTracingKit/decoder/Meta.hpp"
#include "commands/filter.hpp"
#include "commands/interface.hpp"
#include "commands/output.hpp"

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;
using MetaEntryInfo = CommonLowLevelTracingKit::decoder::MetaEntryInfo;
using MetaEntryType = CommonLowLevelTracingKit::decoder::MetaEntryType;
using MetaSourceInfo = CommonLowLevelTracingKit::decoder::MetaSourceInfo;
using MetaSourceInfoCollection = CommonLowLevelTracingKit::decoder::MetaSourceInfoCollection;
using MetaSourceType = CommonLowLevelTracingKit::decoder::MetaSourceType;

static std::string source_type_to_string(MetaSourceType type)
{
	return CommonLowLevelTracingKit::decoder::metaSourceTypeToString(type);
}

static void print_source_header(Output &out, size_t name_width)
{
	out.printf("%-*s %-11s %10s %8s\n", static_cast<int>(name_width), "SOURCE", "TYPE", "META_SIZE",
			   "ENTRIES");
}

static void print_source_row(Output &out, const MetaSourceInfo &info, size_t name_width)
{
	if (info.valid()) {
		out.printf("%-*s %-11s %10lu %8zu\n", static_cast<int>(name_width), info.name.c_str(),
				   source_type_to_string(info.source_type).c_str(), info.meta_size,
				   info.entries.size());
	} else {
		out.printf("%-*s %-11s %10s %8s  [error: %s]\n", static_cast<int>(name_width),
				   info.name.c_str(), source_type_to_string(info.source_type).c_str(), "?", "?",
				   info.error.value_or("unknown").c_str());
	}
}

static void print_entry_header(Output &out, bool full)
{
	if (full) {
		out.printf("  %-7s %6s %-10s %s\n", "TYPE", "LINE", "ARGTYPES", "FILE");
		out.printf("  %s\n", "FORMAT");
	} else {
		out.printf("  %-7s %6s %-25s %-10s %s\n", "TYPE", "LINE", "FILE", "ARGTYPES", "FORMAT");
	}
}

static void print_entry_row(Output &out, const MetaEntryInfo &entry, bool full)
{
	if (full) {
		out.printf("  %-7s %6u %-10s %s\n", MetaEntryInfo::typeToString(entry.type).c_str(),
				   entry.line, entry.arg_types.c_str(), entry.file.c_str());
		out.printf("  %s\n", entry.format.c_str());
	} else {
		std::string format_display = entry.format;
		if (format_display.size() > 50) {
			format_display = format_display.substr(0, 47) + "...";
		}

		std::string file_display = entry.file;
		auto slash = file_display.rfind('/');
		if (slash != std::string::npos) {
			file_display = file_display.substr(slash + 1);
		}
		if (file_display.size() > 25) {
			file_display = "..." + file_display.substr(file_display.size() - 22);
		}

		out.printf("  %-7s %6u %-25s %-10s %s\n", MetaEntryInfo::typeToString(entry.type).c_str(),
				   entry.line, file_display.c_str(), entry.arg_types.c_str(),
				   format_display.c_str());
	}
}

static void print_text_output(Output &out, const MetaSourceInfoCollection &sources,
							  bool show_entries, bool full)
{
	if (sources.empty()) {
		return;
	}

	size_t name_width = 6;
	for (const auto &source : sources) {
		name_width = std::max(name_width, source.name.size());
	}

	print_source_header(out, name_width);

	for (const auto &source : sources) {
		if (is_interrupted()) {
			break;
		}
		print_source_row(out, source, name_width);
		if (show_entries && source.valid() && !source.entries.empty()) {
			print_entry_header(out, full);
			for (const auto &entry : source.entries) {
				if (is_interrupted()) {
					break;
				}
				print_entry_row(out, entry, full);
			}
		}
	}
}

static void print_json_output(Output &out, const MetaSourceInfoCollection &sources, bool pretty)
{
	rapidjson::Document doc;
	doc.SetArray();
	auto &allocator = doc.GetAllocator();

	for (const auto &source : sources) {
		if (is_interrupted()) {
			break;
		}
		rapidjson::Value source_obj;
		source_obj.SetObject();

		rapidjson::Value name;
		name.SetString(source.name.c_str(), allocator);
		source_obj.AddMember("name", name, allocator);

		rapidjson::Value path;
		path.SetString(source.path.string().c_str(), allocator);
		source_obj.AddMember("path", path, allocator);

		rapidjson::Value source_type;
		source_type.SetString(source_type_to_string(source.source_type).c_str(), allocator);
		source_obj.AddMember("source_type", source_type, allocator);

		source_obj.AddMember("meta_size", rapidjson::Value(source.meta_size), allocator);

		if (source.valid()) {
			rapidjson::Value entries_arr;
			entries_arr.SetArray();

			for (const auto &entry : source.entries) {
				rapidjson::Value entry_obj;
				entry_obj.SetObject();

				entry_obj.AddMember("offset", rapidjson::Value(entry.offset), allocator);
				entry_obj.AddMember("size", rapidjson::Value(entry.size), allocator);

				rapidjson::Value type_str;
				type_str.SetString(MetaEntryInfo::typeToString(entry.type).c_str(), allocator);
				entry_obj.AddMember("type", type_str, allocator);

				entry_obj.AddMember("line", rapidjson::Value(entry.line), allocator);
				entry_obj.AddMember("arg_count", rapidjson::Value(entry.arg_count), allocator);

				rapidjson::Value arg_types;
				arg_types.SetString(entry.arg_types.c_str(), allocator);
				entry_obj.AddMember("arg_types", arg_types, allocator);

				rapidjson::Value arg_type_names;
				arg_type_names.SetArray();
				for (const auto &type_name : entry.argumentTypeNames()) {
					rapidjson::Value name_val;
					name_val.SetString(type_name.c_str(), allocator);
					arg_type_names.PushBack(name_val, allocator);
				}
				entry_obj.AddMember("arg_type_names", arg_type_names, allocator);

				rapidjson::Value file;
				file.SetString(entry.file.c_str(), allocator);
				entry_obj.AddMember("file", file, allocator);

				rapidjson::Value format;
				format.SetString(entry.format.c_str(), allocator);
				entry_obj.AddMember("format", format, allocator);

				entries_arr.PushBack(entry_obj, allocator);
			}

			source_obj.AddMember("entries", entries_arr, allocator);
		} else {
			rapidjson::Value error;
			error.SetString(source.error.value_or("unknown error").c_str(), allocator);
			source_obj.AddMember("error", error, allocator);
		}

		doc.PushBack(source_obj, allocator);
	}

	rapidjson::StringBuffer buffer;
	if (pretty) {
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
	} else {
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
	}
	out.printf("%s\n", buffer.GetString());
}

static void add_meta_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("meta", "Show tracepoint metadata definitions");
	command->alias("mt");
	command->description(
		"Display static tracepoint metadata from tracebuffer files or ELF binaries.\n"
		"Shows compile-time information including source location, format strings,\n"
		"and argument types for each tracepoint.\n\n"
		"Supported input types:\n"
		"  - Tracebuffer files (.clltk_trace, .clltk_ktrace)\n"
		"  - ELF executables and shared libraries with CLLTK sections\n"
		"  - Directories (scanned for matching files)\n\n"
		"If no path is specified, uses CLLTK_TRACING_PATH or current directory.");

	static std::string input_path{};
	command
		->add_option("path", input_path,
					 "Path to tracebuffer file, ELF binary, or directory\n"
					 "(default: CLLTK_TRACING_PATH or current directory)")
		->type_name("PATH");

	static bool recursive = true;
	command->add_flag("-r,--recursive,!--no-recursive", recursive,
					  "Recurse into subdirectories (default: yes)");

	static std::string filter_str =
		CommonLowLevelTracingKit::cmd::interface::default_filter_pattern;
	CommonLowLevelTracingKit::cmd::interface::add_filter_option(command, filter_str);

	static bool json_output = false;
	command->add_flag("-j,--json", json_output, "Output as JSON");

	static bool pretty_json = false;
	command->add_flag("-p,--pretty", pretty_json, "Pretty-print JSON output");

	static bool show_entries = true;
	command->add_flag(
		"-s,--summary", [](int64_t) { show_entries = false; },
		"Show only summary (no individual entries)");

	static bool full_width = false;
	command->add_flag("-w,--width", full_width,
					  "Show full width output (no truncation of format strings or file paths)");

	static std::string output_path{};
	command
		->add_option("-o,--output", output_path,
					 "Output file path (default: stdout, use - for stdout)")
		->type_name("FILE");

	static bool compress_output = false;
	command->add_flag("-z,--compress", compress_output, "Compress output with gzip");

	command->callback([&]() {
		std::string resolved_input = input_path.empty() ? get_tracing_path().string() : input_path;

		const boost::regex filter_regex{filter_str};
		auto filter_func = [&](const std::string &name) -> bool {
			return CommonLowLevelTracingKit::cmd::interface::match_tracebuffer_filter(name,
																					  filter_regex);
		};

		auto sources =
			CommonLowLevelTracingKit::decoder::getMetaInfo(resolved_input, recursive, filter_func);

		auto out = create_output(output_path, compress_output);
		if (!out) {
			log_error("Cannot open output: ", output_path.empty() ? "stdout" : output_path);
			throw CLI::RuntimeError(1);
		}

		if (json_output) {
			print_json_output(*out, sources, pretty_json);
		} else {
			if (sources.empty()) {
				log_info("No meta information found in ", resolved_input);
			} else {
				print_text_output(*out, sources, show_entries, full_width);

				if (!is_interrupted()) {
					size_t total_sources = sources.size();
					size_t total_entries = 0;
					size_t error_count = 0;
					for (const auto &source : sources) {
						if (source.valid()) {
							total_entries += source.entries.size();
						} else {
							++error_count;
						}
					}
					out->printf("\nTotal: %zu sources, %zu entries", total_sources, total_entries);
					if (error_count > 0) {
						out->printf(" (%zu errors)", error_count);
					}
					out->printf("\n");
				}
			}
		}

		if (is_interrupted()) {
			log_info("Interrupted");
			return 130;
		}
		return 0;
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_meta_command(app);
}
COMMAND_INIT(init_function);
