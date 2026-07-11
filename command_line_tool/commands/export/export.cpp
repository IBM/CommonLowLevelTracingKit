// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <boost/heap/priority_queue.hpp>
#include <boost/regex.hpp>
#include <cstdio>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "commands/filter.hpp"
#include "commands/interface.hpp"
#include "commands/output.hpp"

using namespace CommonLowLevelTracingKit::cmd::interface;
using Tracebuffer = CommonLowLevelTracingKit::decoder::Tracebuffer;
using SnapTracebuffer = CommonLowLevelTracingKit::decoder::SnapTracebuffer;
using SnapTracebufferPtr = CommonLowLevelTracingKit::decoder::SnapTracebufferPtr;
using Tracepoint = CommonLowLevelTracingKit::decoder::Tracepoint;
using TracepointPtr = CommonLowLevelTracingKit::decoder::TracepointPtr;
using SpanInfo = CommonLowLevelTracingKit::decoder::SpanInfo;

static std::string hex64(uint64_t v)
{
	char buf[19];
	snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(v));
	return buf;
}

static void add_export_command(CLI::App &app)
{
	CLI::App *const command =
		app.add_subcommand("export", "Export trace files as Chrome/Perfetto trace event JSON");
	command->alias("ex");
	command->description(
		"Export one or multiple trace files to Chrome Trace Event Format JSON.\n"
		"Output is compatible with chrome://tracing and Perfetto UI.\n"
		"Supports single tracebuffer files, archives (.clltk snapshots), or directories.");

	static std::vector<std::string> input_paths{};
	command
		->add_option("input", input_paths,
					 "Path(s) to trace data: file, .clltk archive, or directory\n"
					 "(default: CLLTK_TRACING_PATH or current directory)")
		->type_name("PATH");

	static std::string output_path{};
	command
		->add_option("-o,--output", output_path,
					 "Output file path (default: stdout, use - for stdout)")
		->type_name("FILE");

	static bool recursive = true;
	command->add_flag("-r,--recursive,!--no-recursive", recursive,
					  "Recurse into subdirectories (default: yes)");

	static std::string tracebuffer_filter_str =
		CommonLowLevelTracingKit::cmd::interface::default_filter_pattern;
	CommonLowLevelTracingKit::cmd::interface::add_filter_option(command, tracebuffer_filter_str);

	command->callback([&]() {
		std::vector<std::string> resolved_inputs;
		if (input_paths.empty()) {
			resolved_inputs.push_back(get_tracing_path().string());
		} else {
			resolved_inputs = input_paths;
		}

		const bool use_stdout = output_path.empty() || output_path == "-";
		FILE *raw_file = nullptr;
		auto out = create_output(output_path, /*compress=*/false, &raw_file);
		if (!out) {
			log_error("Cannot open output: ", output_path.empty() ? "stdout" : output_path);
			throw CLI::RuntimeError(1);
		}
		OutputFileGuard output_guard(use_stdout ? "" : output_path);

		const boost::regex tb_filter_regex{tracebuffer_filter_str};
		const auto tbFilter = [&](const Tracebuffer &tb) {
			return match_tracebuffer_filter(tb.name(), tb_filter_regex);
		};

		// Collect all tracebuffers across all input paths
		std::vector<SnapTracebufferPtr> all_tbs;
		for (const auto &input : resolved_inputs) {
			auto tbs = SnapTracebuffer::collect(input, tbFilter, {}, recursive);
			for (auto &tb : tbs) {
				all_tbs.emplace_back(std::move(tb));
			}
		}

		// Global sort by timestamp (ascending) using a min-heap via max-heap with inverted compare
		static constexpr auto comp = [](const TracepointPtr &a, const TracepointPtr &b) {
			return a->timestamp_ns > b->timestamp_ns;
		};
		boost::heap::priority_queue<TracepointPtr, boost::heap::compare<decltype(comp)>> pq{comp};
		for (auto &tb : all_tbs) {
			for (auto &tp : tb->tracepoints) {
				pq.emplace(std::move(tp));
			}
		}

		// Track span begin names so span_end events can carry the same name
		std::unordered_map<uint64_t, std::string> span_names;

		rapidjson::Document doc;
		doc.SetObject();
		auto &alloc = doc.GetAllocator();
		rapidjson::Value trace_events(rapidjson::kArrayType);

		while (!pq.empty()) {
			const Tracepoint &tp = *pq.top();
			const double ts_us = static_cast<double>(tp.timestamp_ns) / 1000.0;
			const int pid = static_cast<int>(tp.pid());
			const int tid = static_cast<int>(tp.tid());

			const auto sv_val = [&](std::string_view sv) {
				rapidjson::Value v;
				v.SetString(sv.data(), static_cast<rapidjson::SizeType>(sv.size()), alloc);
				return v;
			};
			const auto str_val = [&](const std::string &s) { return sv_val(std::string_view(s)); };

			auto si = tp.span_info();
			if (si.has_value()) {
				const SpanInfo &s = *si;
				if (s.kind == SpanInfo::Kind::Begin) {
					span_names[s.id] = s.name;
					const std::string id_str = hex64(s.id);

					rapidjson::Value ev(rapidjson::kObjectType);
					ev.AddMember("name", str_val(s.name), alloc);
					ev.AddMember("ph", rapidjson::Value("b", alloc), alloc);
					ev.AddMember("cat", sv_val(tp.tracebuffer()), alloc);
					ev.AddMember("id", str_val(id_str), alloc);
					ev.AddMember("ts", rapidjson::Value(ts_us), alloc);
					ev.AddMember("pid", rapidjson::Value(pid), alloc);
					ev.AddMember("tid", rapidjson::Value(tid), alloc);
					if (s.parent_id != 0) {
						const std::string parent_str = hex64(s.parent_id);
						rapidjson::Value args(rapidjson::kObjectType);
						args.AddMember("parent", str_val(parent_str), alloc);
						ev.AddMember("args", std::move(args), alloc);
					}
					trace_events.PushBack(std::move(ev), alloc);
				} else {
					// Kind::End
					const std::string id_str = hex64(s.id);
					std::string name;
					auto it = span_names.find(s.id);
					if (it != span_names.end())
						name = it->second;

					rapidjson::Value ev(rapidjson::kObjectType);
					ev.AddMember("ph", rapidjson::Value("e", alloc), alloc);
					ev.AddMember("cat", sv_val(tp.tracebuffer()), alloc);
					ev.AddMember("id", str_val(id_str), alloc);
					ev.AddMember("name", str_val(name), alloc);
					ev.AddMember("ts", rapidjson::Value(ts_us), alloc);
					ev.AddMember("pid", rapidjson::Value(pid), alloc);
					ev.AddMember("tid", rapidjson::Value(tid), alloc);
					trace_events.PushBack(std::move(ev), alloc);
				}
			} else {
				// Regular (instant) tracepoint
				rapidjson::Value ev(rapidjson::kObjectType);
				ev.AddMember("name", sv_val(tp.msg()), alloc);
				ev.AddMember("ph", rapidjson::Value("i", alloc), alloc);
				ev.AddMember("s", rapidjson::Value("t", alloc), alloc);
				ev.AddMember("ts", rapidjson::Value(ts_us), alloc);
				ev.AddMember("pid", rapidjson::Value(pid), alloc);
				ev.AddMember("tid", rapidjson::Value(tid), alloc);
				ev.AddMember("cat", sv_val(tp.tracebuffer()), alloc);
				rapidjson::Value args(rapidjson::kObjectType);
				args.AddMember("file", sv_val(tp.file()), alloc);
				args.AddMember("line", rapidjson::Value(static_cast<int64_t>(tp.line())), alloc);
				ev.AddMember("args", std::move(args), alloc);
				trace_events.PushBack(std::move(ev), alloc);
			}

			pq.pop();
		}

		doc.AddMember("displayTimeUnit", rapidjson::Value("ms", alloc), alloc);
		doc.AddMember("traceEvents", std::move(trace_events), alloc);

		rapidjson::StringBuffer buf;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
		doc.Accept(writer);

		out->printf("%s\n", buf.GetString());

		if (!use_stdout && raw_file != nullptr) {
			std::fclose(raw_file);
		}
		out.reset();

		log_verbose("Exported trace events to ",
					output_path.empty() ? std::string("stdout") : output_path);
		return 0;
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_export_command(app);
}
COMMAND_INIT(init_function);
