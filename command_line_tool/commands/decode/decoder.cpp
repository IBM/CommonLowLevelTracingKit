// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <boost/heap/fibonacci_heap.hpp>
#include <boost/heap/priority_queue.hpp>
#include <boost/regex.hpp>
#include <functional>
#include <string>

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "commands/interface.hpp"

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;
using Tracebuffer = CommonLowLevelTracingKit::decoder::Tracebuffer;
using SnapTracebuffer = CommonLowLevelTracingKit::decoder::SnapTracebuffer;
using Tracepoint = CommonLowLevelTracingKit::decoder::Tracepoint;
using TracepointPtr = CommonLowLevelTracingKit::decoder::TracepointPtr;
using TracepointCollection = CommonLowLevelTracingKit::decoder::TracepointCollection;

struct Formattable : public CLI::Validator {
	Formattable(void) : CLI::Validator("Formattable")
	{
		func_ = [](const std::string &filename) -> std::string {
			if (std::filesystem::is_directory(filename))
				return {};
			else if (SnapTracebuffer::is_formattable(filename))
				return {};
			else
				return "\"" + filename + "\"not formattable";
		};
	}
};

void print_tracepoint(FILE *f, int tb_name_size, const Tracepoint &p)
{
	const char *const tb = p.tracebuffer.data();
	const int tb_size = p.tracebuffer.size();
	fprintf(f, " %s | %s | %-*.*s | %5d | %5d | %s | %s | %ld\n", p.timestamp_str().c_str(),
			p.date_and_time_str().c_str(), tb_name_size, tb_size, tb, p.pid(), p.tid(),
			p.msg().data(), p.file().data(), p.line());
}
void print_header(FILE *f, int tb_name_size)
{
	fprintf(f, " %-20s | %-29s | %-*s | %-5s | %-5s | %s | %s | %s\n", "!timestamp", "time",
			tb_name_size, "tracebuffer", "pid", "tid", "formatted", "file", "line");
}

static void add_decode_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("de", "format one or multiple traces");
	command->alias("decode");

	static std::string input_path{};

	command
		->add_option(
			"input", input_path,
			"path to format, could be a file (one trace or multiple as a archive) or directory")
		->envname("CLLTK_TRACING_PATH")
		->check(Formattable{})
		->required()
		->type_name("PATH");

	static std::string output_path = "output.txt";
	static auto output_option = command->add_option("-o,--output", output_path, "Output file path")
	->capture_default_str()
	->type_name("FILE");
	
	static bool use_stdout = false;
	command->add_flag("--stdout", use_stdout, "Output to stdout instead of file")
		->excludes(output_option);

	static std::string tracebuffer_filter_str = "^.*$";
	command
		->add_option("-t,--tracebuffer-filter", tracebuffer_filter_str,
					 "tracebuffer filter as ECMAScript regex")
		->capture_default_str()
		->type_name("REGEX");

	static bool sorted = true;
	command->add_flag("--sorted", sorted, "sort all tracepoints by timestamp");
	command->add_flag("--unsorted", [&](size_t) { sorted = false; }, "do not sort timestamps");

	command->callback([&]() {
		FILE *out = use_stdout ? stdout : std::fopen(output_path.c_str(), "w+");
		if (!use_stdout && !out) {
			std::perror("fopen");
			return 1;
		}
		const boost::regex tracebuffer_filter_regex{tracebuffer_filter_str};
		const auto tbFilter = [&](const Tracebuffer &tb) {
			const bool tracebuffer_used =
				boost::regex_match(tb.name().data(), tracebuffer_filter_regex);
			if (!tracebuffer_used)
				return false;
			return true;
		};
		auto tbs = SnapTracebuffer::collect(input_path, tbFilter);

		size_t tb_name_size = 0;
		for (const auto &tb : tbs)
			tb_name_size = std::max(tb_name_size, tb->name().size());

		print_header(out, tb_name_size);
		if (sorted) {
			static constexpr auto comp = [](const TracepointPtr &a, const TracepointPtr &b) {
				return a->timestamp_ns > b->timestamp_ns;
			};
			boost::heap::priority_queue<TracepointPtr, boost::heap::compare<decltype(comp)>> tps{
				comp};
			for (const auto &tb : tbs) {
				TracepointCollection &tb_tps = tb->tracepoints;
				for (auto &tp : tb_tps)
					tps.emplace(std::move(tp));
			}
			while (!tps.empty()) {
				const auto tp = tps.top().get();
				print_tracepoint(out, tb_name_size, *tp);
				tps.pop();
			}
		} else {
			for (const auto &tb : tbs) {
				for (const auto &tp : tb->tracepoints)
					print_tracepoint(out, tb_name_size, *tp);
			}
		}

		if (!use_stdout)
			std::fclose(out);
		return 0;
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_decode_command(app);
}
COMMAND_INIT(init_function);
