// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/regex.hpp>

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "commands/interface.hpp"
#include "ordered_buffer.hpp"

// Include pool header for TracepointPool (from decoder_tool source)
#include "pool.hpp"
#include "to_string.hpp"

using namespace std::chrono_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;
namespace live = CommonLowLevelTracingKit::cmd::live;
using SyncTracebuffer = CommonLowLevelTracingKit::decoder::SyncTracebuffer;
using SyncTracebufferPtr = CommonLowLevelTracingKit::decoder::SyncTracebufferPtr;
using Tracepoint = CommonLowLevelTracingKit::decoder::Tracepoint;
using TracepointPtr = CommonLowLevelTracingKit::decoder::TracepointPtr;
using TracepointPool = CommonLowLevelTracingKit::decoder::source::TracepointPool;
using ToString = CommonLowLevelTracingKit::decoder::source::low_level::ToString;

namespace
{

std::atomic<int> g_signal_count{0};
std::atomic<bool> g_stop_requested{false};

void signal_handler(int sig)
{
	int count = ++g_signal_count;
	g_stop_requested.store(true, std::memory_order_release);

	if (count >= 2) {
		std::quick_exit(128 + sig);
	}
}

void reset_signal_state()
{
	g_signal_count.store(0, std::memory_order_release);
	g_stop_requested.store(false, std::memory_order_release);
}

/**
 * Live streaming decoder for real-time tracepoint monitoring.
 * Reader thread polls buffers, output thread emits in timestamp order.
 */
class LiveDecoder
{
  public:
	struct Config {
		std::filesystem::path input_path;
		std::string tracebuffer_filter = "^.*$";
		size_t buffer_size = 100000;
		uint64_t order_delay_ms = 25;
		uint64_t poll_interval_ms = 5;
		bool show_summary = false;
		FILE *output = stdout;
	};

	explicit LiveDecoder(Config config)
		: m_config(std::move(config)),
		  m_buffer(m_config.buffer_size, m_config.order_delay_ms * 1'000'000),
		  m_pool(4) // Start with 4 blocks = 4096 tracepoint slots
	{
	}

	~LiveDecoder() { stop(); }

	bool start() // false if already running or no buffers found
	{
		if (m_running.exchange(true)) {
			return false;
		}

		if (!discover_tracebuffers()) {
			m_running = false;
			return false;
		}

		if (m_tracebuffers.empty()) {
			std::cerr << "No tracebuffers found matching filter" << std::endl;
			m_running = false;
			return false;
		}

		std::cerr << "Monitoring " << m_tracebuffers.size() << " tracebuffer(s)..." << std::endl;

		for (const auto &tb : m_tracebuffers) {
			m_tb_name_width = std::max(m_tb_name_width, tb->name().size());
		}

		print_header();

		m_reader_thread = std::thread(&LiveDecoder::reader_loop, this);
		m_output_thread = std::thread(&LiveDecoder::output_loop, this);

		return true;
	}

	void stop() // waits for flush
	{
		if (!m_running.exchange(false)) {
			return;
		}

		m_stop_reader.store(true, std::memory_order_release);

		if (m_reader_thread.joinable()) {
			m_reader_thread.join();
		}
		m_buffer.finish();

		if (m_output_thread.joinable()) {
			m_output_thread.join();
		}

		if (m_config.show_summary) {
			print_summary();
		}
	}

	[[nodiscard]] bool running() const noexcept
	{
		return m_running.load(std::memory_order_acquire);
	}

  private:
	/**
	 * @brief Discover tracebuffers matching the filter
	 */
	bool discover_tracebuffers()
	{
		const boost::regex filter_regex{m_config.tracebuffer_filter};

		try {
			if (std::filesystem::is_directory(m_config.input_path)) {
				// Scan directory for tracebuffers
				for (const auto &entry : std::filesystem::directory_iterator(m_config.input_path)) {
					if (SyncTracebuffer::is_tracebuffer(entry.path())) {
						auto tb = SyncTracebuffer::make(entry.path());
						if (tb && boost::regex_match(tb->name().data(), filter_regex)) {
							m_tracebuffers.push_back(std::move(tb));
						}
					}
				}
			} else if (SyncTracebuffer::is_tracebuffer(m_config.input_path)) {
				// Single tracebuffer file
				auto tb = SyncTracebuffer::make(m_config.input_path);
				if (tb && boost::regex_match(tb->name().data(), filter_regex)) {
					m_tracebuffers.push_back(std::move(tb));
				}
			} else {
				std::cerr << "Invalid input path: " << m_config.input_path << std::endl;
				return false;
			}
		} catch (const std::exception &e) {
			std::cerr << "Error discovering tracebuffers: " << e.what() << std::endl;
			return false;
		}

		return true;
	}

	/**
	 * @brief Reader thread loop
	 *
	 * Polls tracebuffers prioritized by pending count, pushes to ordered buffer.
	 */
	void reader_loop()
	{
		const auto poll_interval = std::chrono::milliseconds(m_config.poll_interval_ms);
		const uint64_t order_delay_ns = m_config.order_delay_ms * 1'000'000;

		while (!m_stop_reader.load(std::memory_order_acquire) &&
			   !g_stop_requested.load(std::memory_order_acquire)) {

			bool any_pending = false;
			uint64_t max_seen_ts = 0;

			// Sort tracebuffers by pending count (busiest first)
			std::vector<std::pair<uint64_t, size_t>> pending_counts;
			pending_counts.reserve(m_tracebuffers.size());

			for (size_t i = 0; i < m_tracebuffers.size(); ++i) {
				uint64_t pending = m_tracebuffers[i]->pending();
				if (pending > 0) {
					pending_counts.emplace_back(pending, i);
					any_pending = true;
				}
			}

			// Sort descending by pending count
			std::sort(pending_counts.begin(), pending_counts.end(),
					  [](const auto &a, const auto &b) { return a.first > b.first; });

			// Process tracebuffers in priority order
			for (const auto &[pending, idx] : pending_counts) {
				auto &tb = m_tracebuffers[idx];

				// Read all pending tracepoints from this buffer
				while (auto tp = tb->next_pooled(m_pool)) {
					uint64_t ts = tp->timestamp_ns;
					if (ts > max_seen_ts) {
						max_seen_ts = ts;
					}

					m_buffer.push(std::move(tp));
					++m_total_read;

					// Check for stop signal periodically
					if (g_stop_requested.load(std::memory_order_acquire)) {
						break;
					}
				}

				if (g_stop_requested.load(std::memory_order_acquire)) {
					break;
				}
			}

			// Update watermark to allow tracepoints to be released.
			// The watermark logic: tracepoints with timestamp <= (watermark - delay) are released.
			//
			// When we read tracepoints, we track the max timestamp seen (m_max_watermark_ts).
			// When idle (no pending), we know no older tracepoints will arrive, so we can
			// advance the watermark past our buffered tracepoints to release them.
			//
			// Key insight: we need to add delay to max_seen_ts so that:
			//   safe_threshold = watermark - delay = max_seen_ts + delay - delay = max_seen_ts
			// This releases all tracepoints with timestamp <= max_seen_ts (i.e., all of them).
			if (max_seen_ts > 0) {
				// Track the maximum timestamp we've ever seen
				if (max_seen_ts > m_max_watermark_ts) {
					m_max_watermark_ts = max_seen_ts;
				}
			}

			if (!any_pending) {
				// No pending tracepoints - safe to release everything we've buffered.
				// Set watermark to (max_seen + delay) so that safe_threshold = max_seen,
				// which releases all tracepoints up to and including max_seen.
				if (m_max_watermark_ts > 0) {
					// Protect against overflow (unlikely but possible with large timestamps)
					const uint64_t watermark = (m_max_watermark_ts <= UINT64_MAX - order_delay_ns)
												   ? (m_max_watermark_ts + order_delay_ns)
												   : UINT64_MAX;
					m_buffer.update_watermark(watermark);
				}
				std::this_thread::sleep_for(poll_interval);
			} else {
				// Have pending tracepoints - set watermark to max seen so far.
				// This allows older tracepoints to be released while keeping recent ones
				// buffered in case out-of-order tracepoints arrive from other buffers.
				m_buffer.update_watermark(m_max_watermark_ts);
			}
		}

		// Final flush - signal buffer that reader is done
		m_buffer.finish();
	}

	/**
	 * @brief Output thread loop
	 *
	 * Waits for tracepoints to become ready (based on watermark), outputs them.
	 * Uses batched flushing for better I/O performance.
	 */
	void output_loop()
	{
		while (!m_buffer.finished()) {
			// Pop all ready tracepoints
			auto ready = m_buffer.pop_all_ready();
			for (auto &tp : ready) {
				print_tracepoint(*tp);
				++m_total_output;
			}

			// Flush after processing a batch, not after each tracepoint
			if (!ready.empty()) {
				fflush(m_config.output);
			} else {
				// If nothing ready, wait a bit
				std::this_thread::sleep_for(10ms);
			}
		}
	}

	void print_header()
	{
		fprintf(m_config.output, " %-20s | %-29s | %-*s | %-5s | %-5s | %s | %s | %s\n",
				"!timestamp", "time", static_cast<int>(m_tb_name_width), "tracebuffer", "pid",
				"tid", "formatted", "file", "line");
	}

	void print_tracepoint(const Tracepoint &p)
	{
		const auto tb_view = p.tracebuffer();
		const char *const tb = tb_view.data();
		const int tb_size = static_cast<int>(tb_view.size());
		const int tb_width = static_cast<int>(m_tb_name_width);

		// Use stack buffers for timestamp formatting (no allocation)
		char ts_buf[ToString::TIMESTAMP_NS_BUF_SIZE];
		char dt_buf[ToString::DATE_AND_TIME_BUF_SIZE];
		const char *ts_str = ToString::timestamp_ns_to(ts_buf, p.timestamp_ns);
		const char *dt_str = ToString::date_and_time_to(dt_buf, p.timestamp_ns);

		// Add '*' prefix for kernel traces
		if (p.is_kernel()) {
			fprintf(m_config.output, " %s | %s | *%-*.*s | %5d | %5d | %s | %s | %ld\n", ts_str,
					dt_str, tb_width - 1, tb_size, tb, p.pid(), p.tid(), p.msg().data(),
					p.file().data(), p.line());
		} else {
			fprintf(m_config.output, " %s | %s | %-*.*s | %5d | %5d | %s | %s | %ld\n", ts_str,
					dt_str, tb_width, tb_size, tb, p.pid(), p.tid(), p.msg().data(),
					p.file().data(), p.line());
		}
		// Note: fflush is now done in output_loop after processing a batch
	}

	void print_summary()
	{
		auto stats = m_buffer.stats();
		std::cerr << "\n--- Live Decoder Summary ---" << std::endl;
		std::cerr << "Tracepoints read:    " << m_total_read << std::endl;
		std::cerr << "Tracepoints output:  " << m_total_output << std::endl;
		std::cerr << "Tracepoints dropped: " << stats.total_dropped << std::endl;
		std::cerr << "Buffer high water:   " << stats.high_water_mark << std::endl;
		std::cerr << "Pool allocated:      " << m_pool.allocated() << std::endl;
		std::cerr << "Pool capacity:       " << m_pool.capacity() << std::endl;
	}

	Config m_config;
	live::OrderedBuffer m_buffer;
	TracepointPool m_pool;

	std::vector<SyncTracebufferPtr> m_tracebuffers;
	size_t m_tb_name_width{11}; // "tracebuffer"

	std::atomic<bool> m_running{false};
	std::atomic<bool> m_stop_reader{false};

	std::thread m_reader_thread;
	std::thread m_output_thread;

	uint64_t m_total_read{0};
	uint64_t m_total_output{0};
	uint64_t m_max_watermark_ts{0}; // Maximum timestamp seen across all tracepoints
};

} // anonymous namespace

static void add_live_command(CLI::App &app)
{
	CLI::App *const command =
		app.add_subcommand("live", "Live streaming decoder for real-time trace monitoring");
	command->alias("lv");
	command->description(
		"Monitor tracebuffers in real-time and output tracepoints as they arrive.\n"
		"Uses a reader thread to poll tracebuffers and an output thread for ordered display.\n"
		"If no input is specified, uses CLLTK_TRACING_PATH or current directory.\n"
		"Supports graceful shutdown via Ctrl+C (SIGINT/SIGTERM). Press twice to force exit.");

	// Use static variables for CLI option storage, but reset defaults in callback
	static std::string input_path{};
	static std::string tracebuffer_filter_str{};
	static size_t buffer_size{};
	static uint64_t order_delay_ms{};
	static uint64_t poll_interval_ms{};
	static bool show_summary{};
	static bool recursive{};

	// Default values (used for display and reset)
	constexpr const char *default_filter = "^.*$";
	constexpr size_t default_buffer_size = 100000;
	constexpr uint64_t default_order_delay_ms = 25;
	constexpr uint64_t default_poll_interval_ms = 5;

	command
		->add_option("input", input_path,
					 "Path to tracebuffer file or directory to monitor\n"
					 "(default: CLLTK_TRACING_PATH or current directory)")
		->type_name("PATH");

	command->add_flag("-r,--recursive,!--no-recursive", recursive,
					  "Recurse into subdirectories (default: no)");

	command
		->add_option("-F,--filter", tracebuffer_filter_str,
					 "Filter tracebuffers by name using regex")
		->default_val(default_filter)
		->type_name("REGEX");

	command
		->add_option("--buffer-size", buffer_size,
					 "Maximum tracepoints to buffer in memory.\n"
					 "Older tracepoints are dropped when limit is reached.\n"
					 "Set to 0 for unlimited (may consume large memory)")
		->default_val(default_buffer_size)
		->type_name("SIZE");

	command
		->add_option("--order-delay", order_delay_ms,
					 "Delay window in milliseconds for timestamp ordering.\n"
					 "Higher values improve ordering accuracy but increase latency.\n"
					 "Tracepoints are held until this delay passes to allow reordering")
		->default_val(default_order_delay_ms)
		->type_name("MS");

	command
		->add_option("--poll-interval", poll_interval_ms,
					 "Poll interval in milliseconds when no tracepoints are pending.\n"
					 "Lower values reduce latency but increase CPU usage")
		->default_val(default_poll_interval_ms)
		->type_name("MS");

	command->add_flag("-S,--summary", show_summary,
					  "Show statistics summary on exit (read/output/dropped counts, buffer usage)");

	command->callback([&]() {
		// Reset global signal state (for multiple runs in same process)
		reset_signal_state();

		// Resolve input path: use provided path, or fall back to tracing path
		std::string resolved_input = input_path.empty() ? get_tracing_path().string() : input_path;

		// Save previous signal handlers to restore later
		struct sigaction old_sigint{}, old_sigterm{};

		// Install signal handlers
		struct sigaction sa{};
		sa.sa_handler = signal_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGINT, &sa, &old_sigint);
		sigaction(SIGTERM, &sa, &old_sigterm);

		// Configure decoder
		LiveDecoder::Config config;
		config.input_path = resolved_input;
		config.tracebuffer_filter = tracebuffer_filter_str;
		config.buffer_size = buffer_size;
		config.order_delay_ms = order_delay_ms;
		config.poll_interval_ms = poll_interval_ms;
		config.show_summary = show_summary;
		config.output = stdout;

		// Create and run decoder
		LiveDecoder decoder(config);

		if (!decoder.start()) {
			sigaction(SIGINT, &old_sigint, nullptr);
			sigaction(SIGTERM, &old_sigterm, nullptr);
			show_summary = false;
			return;
		}

		// Wait for completion (will be interrupted by Ctrl+C)
		while (decoder.running() && !g_stop_requested.load(std::memory_order_acquire)) {
			std::this_thread::sleep_for(100ms);
		}

		// Stop decoder (handles graceful shutdown)
		decoder.stop();

		// Restore previous signal handlers
		sigaction(SIGINT, &old_sigint, nullptr);
		sigaction(SIGTERM, &old_sigterm, nullptr);

		// Reset static variables for next run (show_summary is a flag, reset to false)
		show_summary = false;
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_live_command(app);
}
COMMAND_INIT(init_function);
