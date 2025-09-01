#ifndef DECODER_TOOL_TRACEBUFFER_HEADER
#define DECODER_TOOL_TRACEBUFFER_HEADER
#include <filesystem>
#include <functional>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

#include "Common.hpp"
#include "Tracepoint.hpp"

namespace CommonLowLevelTracingKit::decoder {
	struct Tracebuffer;
	using TracepointFilterFunc = std::function<bool(const Tracepoint &)>;
	using TracebufferFilterFunc = std::function<bool(const Tracebuffer &)>;

	struct EXPORT Tracebuffer {
		virtual ~Tracebuffer() noexcept = default;

		virtual const std::string_view name() const noexcept = 0;
		virtual size_t size() const noexcept = 0;
		virtual const std::filesystem::path &path() const noexcept { return m_path; };
		virtual bool is_user_space() const noexcept {
			return m_path.extension() == std::string(".clltk_trace");
		};

		static bool is_tracebuffer(const std::filesystem::path &);

	  protected:
		Tracebuffer(const std::filesystem::path &path) noexcept
			: m_path(path) {};
		const std::filesystem::path m_path;
	};

	// Synchonous Tracebuffer
	struct SyncTracebuffer;
	using SyncTracebufferPtr = std::unique_ptr<SyncTracebuffer>;
	struct EXPORT SyncTracebuffer : Tracebuffer {
		static SyncTracebufferPtr make(const std::filesystem::path &);
		~SyncTracebuffer() noexcept override = default;
		[[nodiscard]] virtual uint64_t pending() noexcept = 0;
		virtual uint64_t current_top_entries_nr() const noexcept = 0;
		[[nodiscard]] virtual TracepointPtr
		next(const TracepointFilterFunc & = TracepointFilterFunc{}) = 0;

	  protected:
		SyncTracebuffer(const std::filesystem::path &path) noexcept
			: Tracebuffer(path) {};
	};

	struct SnapTracebuffer;
	using SnapTracebufferPtr = std::unique_ptr<SnapTracebuffer>;
	using SnapTracebufferCollection = std::vector<SnapTracebufferPtr>;
	struct EXPORT SnapTracebuffer : Tracebuffer {
		static SnapTracebufferCollection collect(const std::filesystem::path &,
												 TracebufferFilterFunc tbFilter = {},
												 TracepointFilterFunc tpFilter = {});
		static SnapTracebufferPtr make(const std::filesystem::path &,
									   TracepointFilterFunc tpFilte = {});

		TracepointCollection tracepoints;

		virtual const std::string_view name() const noexcept { return m_name; }
		virtual size_t size() const noexcept { return m_size; }

		// is path, directory, clltk user or kernel trace or is compressed or uncompressed tar
		static bool is_formattable(const std::filesystem::path &);

		SnapTracebuffer(const SnapTracebuffer &) noexcept = default;
		SnapTracebuffer(SnapTracebuffer &&) noexcept = default;
		SnapTracebuffer &operator=(const SnapTracebuffer &) noexcept = delete;
		SnapTracebuffer &operator=(SnapTracebuffer &&) noexcept = delete;

		virtual ~SnapTracebuffer() noexcept = default;

	  protected:
		SnapTracebuffer(const std::filesystem::path &, TracepointCollection &&, std::string &&,
						size_t) noexcept;
		const std::string m_name;
		const uint64_t m_size;
	};

} // namespace CommonLowLevelTracingKit::decoder

#endif