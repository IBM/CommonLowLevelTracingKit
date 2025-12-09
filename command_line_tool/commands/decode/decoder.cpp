// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <boost/heap/fibonacci_heap.hpp>
#include <boost/heap/priority_queue.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "commands/interface.hpp"
#include "filter.hpp"
#include "timespec.hpp"

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;
namespace decode = CommonLowLevelTracingKit::cmd::decode;
using Tracebuffer = CommonLowLevelTracingKit::decoder::Tracebuffer;
using SnapTracebuffer = CommonLowLevelTracingKit::decoder::SnapTracebuffer;
using SourceType = CommonLowLevelTracingKit::decoder::SourceType;
using Tracepoint = CommonLowLevelTracingKit::decoder::Tracepoint;
using TracepointPtr = CommonLowLevelTracingKit::decoder::TracepointPtr;
using TracepointCollection = CommonLowLevelTracingKit::decoder::TracepointCollection;

struct Formattable : public CLI::Validator {
	Formattable(void) : CLI::Validator("Formattable")
	{
		func_ = [](const std::string &filename) -> std::string {
			if (std::filesystem::is_directory(filename))
				return {};
			if (SnapTracebuffer::is_formattable(filename))
				return {};
			return "\"" + filename + "\"not formattable";
		};
	}
};

void print_tracepoint(FILE *f, int tb_name_size, const Tracepoint &p)
{
	const char *const tb = p.tracebuffer.data();
	const int tb_size = p.tracebuffer.size();
	// Add '*' prefix for kernel traces in the tracebuffer name column
	const auto msg = p.msg();
	const auto file = p.file();
	if (p.is_kernel()) {
		fprintf(f, " %s | %s | *%-*.*s | %5d | %5d | %.*s | %.*s | %ld\n",
				p.timestamp_str().c_str(), p.date_and_time_str().c_str(), tb_name_size - 1, tb_size,
				tb, p.pid(), p.tid(), static_cast<int>(msg.size()), msg.data(),
				static_cast<int>(file.size()), file.data(), p.line());
	} else {
		fprintf(f, " %s | %s | %-*.*s | %5d | %5d | %.*s | %.*s | %ld\n", p.timestamp_str().c_str(),
				p.date_and_time_str().c_str(), tb_name_size, tb_size, tb, p.pid(), p.tid(),
				static_cast<int>(msg.size()), msg.data(), static_cast<int>(file.size()),
				file.data(), p.line());
	}
}
void print_header(FILE *f, int tb_name_size)
{
	fprintf(f, " %-20s | %-29s | %-*s | %-5s | %-5s | %s | %s | %s\n", "!timestamp", "time",
			tb_name_size, "tracebuffer", "pid", "tid", "formatted", "file", "line");
}

static void add_decode_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("de", "Decode and format trace files");
	command->alias("decode");
	command->description(
		"Decode and format one or multiple trace files into human-readable output.\n"
		"Supports single tracebuffer files, archives (.clltk snapshots), or directories.\n"
		"Use for post-mortem analysis of collected trace data.");

	static std::string input_path{};

	command
		->add_option("input", input_path,
					 "Path to trace data: single tracebuffer file, .clltk archive, or directory")
		->envname("CLLTK_TRACING_PATH")
		->check(Formattable{})
		->required()
		->type_name("PATH");

	static std::string output_path = "output.txt";
	static auto output_option =
		command->add_option("-o,--output", output_path, "Output file path for decoded traces")
			->capture_default_str()
			->type_name("FILE");

	static bool use_stdout = false;
	command
		->add_flag("--stdout", use_stdout,
				   "Output to stdout instead of file (mutually exclusive with -o)")
		->excludes(output_option);

	static std::string tracebuffer_filter_str = "^.*$";
	command
		->add_option("-T,--tracebuffer-filter", tracebuffer_filter_str,
					 "Filter tracebuffers by name using ECMAScript regex")
		->capture_default_str()
		->type_name("REGEX");

	static bool sorted = true;
	command->add_flag("--sorted", sorted,
					  "Sort all tracepoints globally by timestamp (default: true)");
	command->add_flag(
		"--unsorted", [&](size_t) { sorted = false; },
		"Disable sorting; output in per-buffer order (faster for large traces)");

	// Source filter (tracebuffer level)
	static std::string source_filter_str = "all";
	command
		->add_option("--source", source_filter_str,
					 "Filter by trace source type:\n"
					 "  all       - include all traces\n"
					 "  userspace - userspace traces only\n"
					 "  kernel    - kernel traces (includes TTY)\n"
					 "  tty       - kernel TTY traces only")
		->capture_default_str()
		->check(CLI::IsMember({"all", "userspace", "kernel", "tty"}, CLI::ignore_case))
		->type_name("SOURCE");

	// Tracepoint filters
	static std::vector<uint32_t> filter_pids;
	static std::vector<uint32_t> filter_tids;
	static std::string filter_msg;
	static std::string filter_msg_regex;
	static std::string filter_file;
	static std::string filter_file_regex;
	static std::string filter_time_min_str;
	static std::string filter_time_max_str;

	command
		->add_option("--pid", filter_pids,
					 "Filter by process ID(s). Can be specified multiple times")
		->type_name("PID");
	command
		->add_option("--tid", filter_tids,
					 "Filter by thread ID(s). Can be specified multiple times")
		->type_name("TID");
	command->add_option("--msg", filter_msg, "Filter tracepoints containing this message substring")
		->type_name("TEXT");
	command
		->add_option("--msg-regex", filter_msg_regex,
					 "Filter tracepoints by message using ECMAScript regex")
		->type_name("REGEX");
	command
		->add_option("--file", filter_file,
					 "Filter tracepoints from files containing this path substring")
		->type_name("TEXT");
	command
		->add_option("--file-regex", filter_file_regex,
					 "Filter tracepoints by source file path using ECMAScript regex")
		->type_name("REGEX");
	command
		->add_option("--time-min", filter_time_min_str,
					 "Minimum time filter. Formats:\n"
					 "  1234567890.5    - Unix timestamp (seconds)\n"
					 "  2025-11-25T21:46:29 - ISO 8601 datetime\n"
					 "  now, now-1m     - relative to current time\n"
					 "  min, min+1h     - relative to trace start\n"
					 "  -30s            - relative to trace end\n"
					 "Duration suffixes: ns, us, ms, s, m, h")
		->type_name("TIME");
	command
		->add_option("--time-max", filter_time_max_str,
					 "Maximum time filter (same formats as --time-min)")
		->type_name("TIME");

	command->callback([&]() {
		FILE *out = use_stdout ? stdout : std::fopen(output_path.c_str(), "w+");
		if (!use_stdout && !out) {
			std::perror("fopen");
			return 1;
		}

		// Register output file for cleanup on interrupt
		OutputFileGuard output_guard(use_stdout ? "" : output_path);

		// Build tracebuffer filter
		const boost::regex tracebuffer_filter_regex{tracebuffer_filter_str};

		// Parse source filter
		enum class SourceFilter { All, Userspace, Kernel, TTY };
		SourceFilter source_filter = SourceFilter::All;
		{
			std::string src_lower = source_filter_str;
			std::transform(src_lower.begin(), src_lower.end(), src_lower.begin(), ::tolower);
			if (src_lower == "userspace")
				source_filter = SourceFilter::Userspace;
			else if (src_lower == "kernel")
				source_filter = SourceFilter::Kernel;
			else if (src_lower == "tty")
				source_filter = SourceFilter::TTY;
		}

		const auto tbFilter = [&](const Tracebuffer &tb) {
			// Check tracebuffer name regex
			const bool tracebuffer_used =
				boost::regex_match(tb.name().data(), tracebuffer_filter_regex);
			if (!tracebuffer_used)
				return false;

			// Check source type filter
			if (source_filter != SourceFilter::All) {
				switch (source_filter) {
				case SourceFilter::Userspace:
					if (!tb.is_user_space())
						return false;
					break;
				case SourceFilter::Kernel:
					// Kernel includes all kernel traces (including TTY)
					if (!tb.is_kernel_space())
						return false;
					break;
				case SourceFilter::TTY:
					// TTY is a special case of kernel trace
					if (!tb.is_tty())
						return false;
					break;
				default:
					break;
				}
			}

			return true;
		};

		// Parse time specifications
		decode::TimeSpec time_min_spec, time_max_spec;
		time_max_spec.anchor = decode::TimeSpec::Anchor::Absolute;
		time_max_spec.absolute_ns = UINT64_MAX;

		if (!filter_time_min_str.empty()) {
			try {
				time_min_spec = decode::TimeSpec::parse(filter_time_min_str);
			} catch (const std::invalid_argument &e) {
				std::cerr << "Invalid --time-min: " << e.what() << std::endl;
				return 1;
			}
		}
		if (!filter_time_max_str.empty()) {
			try {
				time_max_spec = decode::TimeSpec::parse(filter_time_max_str);
			} catch (const std::invalid_argument &e) {
				std::cerr << "Invalid --time-max: " << e.what() << std::endl;
				return 1;
			}
		}

		// Build tracepoint filter (without time initially if we need bounds)
		decode::TracepointFilter tpFilter;
		tpFilter.pids.insert(filter_pids.begin(), filter_pids.end());
		tpFilter.tids.insert(filter_tids.begin(), filter_tids.end());
		if (!filter_msg_regex.empty())
			tpFilter.set_msg_filter(filter_msg_regex, true);
		else if (!filter_msg.empty())
			tpFilter.set_msg_filter(filter_msg, false);
		if (!filter_file_regex.empty())
			tpFilter.set_file_filter(filter_file_regex, true);
		else if (!filter_file.empty())
			tpFilter.set_file_filter(filter_file, false);

		// Collect tracebuffers (without time filter for now)
		auto tbs = SnapTracebuffer::collect(input_path, tbFilter);

		// Find trace time bounds
		uint64_t trace_min_ns = UINT64_MAX;
		uint64_t trace_max_ns = 0;
		for (const auto &tb : tbs) {
			for (const auto &tp : tb->tracepoints) {
				trace_min_ns = std::min(trace_min_ns, tp->timestamp_ns);
				trace_max_ns = std::max(trace_max_ns, tp->timestamp_ns);
			}
		}

		// Get current time
		auto now = std::chrono::system_clock::now();
		uint64_t now_ns = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

		// Resolve time specifications
		tpFilter.time_min = time_min_spec.resolve(now_ns, trace_min_ns, trace_max_ns);
		tpFilter.time_max = time_max_spec.resolve(now_ns, trace_min_ns, trace_max_ns);
		tpFilter.configure();

		size_t tb_name_size = 0;
		for (const auto &tb : tbs)
			tb_name_size = std::max(tb_name_size, tb->name().size());

		print_header(out, tb_name_size);
		size_t tp_count = 0;
		if (sorted) {
			static constexpr auto comp = [](const TracepointPtr &a, const TracepointPtr &b) {
				return a->timestamp_ns > b->timestamp_ns;
			};
			boost::heap::priority_queue<TracepointPtr, boost::heap::compare<decltype(comp)>> tps{
				comp};
			for (const auto &tb : tbs) {
				if (is_interrupted())
					break;
				TracepointCollection &tb_tps = tb->tracepoints;
				for (auto &tp : tb_tps) {
					if (tpFilter(*tp))
						tps.emplace(std::move(tp));
				}
			}
			while (!tps.empty() && !is_interrupted()) {
				const auto tp = tps.top().get();
				print_tracepoint(out, tb_name_size, *tp);
				tps.pop();
				++tp_count;
			}
		} else {
			for (const auto &tb : tbs) {
				if (is_interrupted())
					break;
				for (const auto &tp : tb->tracepoints) {
					if (is_interrupted())
						break;
					if (tpFilter(*tp)) {
						print_tracepoint(out, tb_name_size, *tp);
						++tp_count;
					}
				}
			}
		}

		if (!use_stdout)
			std::fclose(out);

		if (is_interrupted()) {
			log_info("Interrupted after ", tp_count, " tracepoints");
			return 130; // Standard exit code for SIGINT
		}

		log_verbose("Decoded ", tp_count, " tracepoints to ", output_path);
		return 0;
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_decode_command(app);
}
COMMAND_INIT(init_function);
