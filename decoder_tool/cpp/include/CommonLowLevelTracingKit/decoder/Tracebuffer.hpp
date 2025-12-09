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

namespace CommonLowLevelTracingKit::decoder::source {
	// Forward declaration for pool type - actual definition in pool.hpp
	class TracepointPool;
} // namespace CommonLowLevelTracingKit::decoder::source

namespace CommonLowLevelTracingKit::decoder {

	// SourceType is defined in Tracepoint.hpp

	struct Tracebuffer;
	using TracepointFilterFunc = std::function<bool(const Tracepoint &)>;
	using TracebufferFilterFunc = std::function<bool(const Tracebuffer &)>;

	struct EXPORT Tracebuffer {
		virtual ~Tracebuffer() noexcept = default;

		virtual const std::string_view name() const noexcept = 0;
		virtual size_t size() const noexcept = 0;
		virtual const std::filesystem::path &path() const noexcept { return m_path; };

		/**
		 * @brief Get source type from definition section (V2) or infer from extension (V1)
		 */
		virtual SourceType sourceType() const noexcept { return m_source_type; }

		/**
		 * @brief Check if trace is from userspace (for backwards compatibility)
		 */
		virtual bool is_user_space() const noexcept {
			if (m_source_type == SourceType::Unknown) {
				// Fall back to extension-based detection for V1 files
				return m_path.extension() == std::string(".clltk_trace");
			}
			return m_source_type == SourceType::Userspace;
		};

		/**
		 * @brief Check if trace is from kernel space
		 */
		virtual bool is_kernel_space() const noexcept {
			if (m_source_type == SourceType::Unknown) {
				// Fall back to extension-based detection for V1 files
				return m_path.extension() == std::string(".clltk_ktrace");
			}
			return m_source_type == SourceType::Kernel || m_source_type == SourceType::TTY;
		};

		/**
		 * @brief Check if trace is TTY (kernel trace with TTY buffer name)
		 */
		virtual bool is_tty() const noexcept {
			if (m_source_type == SourceType::TTY) { return true; }
			// Also check by name for V1 files or if source_type wasn't set correctly
			return name() == "TTY" && is_kernel_space();
		};

		static bool is_tracebuffer(const std::filesystem::path &);

	  protected:
		Tracebuffer(const std::filesystem::path &path,
					SourceType source_type = SourceType::Unknown) noexcept
			: m_path(path)
			, m_source_type(source_type) {};
		const std::filesystem::path m_path;
		const SourceType m_source_type;
	};

	// Synchronous Tracebuffer
	struct SyncTracebuffer;
	using SyncTracebufferPtr = std::unique_ptr<SyncTracebuffer>;
	struct EXPORT SyncTracebuffer : Tracebuffer {
		static SyncTracebufferPtr make(const std::filesystem::path &);
		~SyncTracebuffer() noexcept override = default;
		[[nodiscard]] virtual uint64_t pending() noexcept = 0;
		virtual uint64_t current_top_entries_nr() const noexcept = 0;

		/**
		 * @brief Get next tracepoint using heap allocation
		 */
		[[nodiscard]] virtual TracepointPtr
		next(const TracepointFilterFunc & = TracepointFilterFunc{}) noexcept = 0;

		/**
		 * @brief Get next tracepoint using pool allocation
		 *
		 * Allocates from the provided pool for better performance in live streaming.
		 * Falls back to heap allocation if pool is exhausted.
		 */
		[[nodiscard]] virtual TracepointPtr
		next_pooled(source::TracepointPool &pool,
					const TracepointFilterFunc & = TracepointFilterFunc{}) noexcept = 0;

	  protected:
		SyncTracebuffer(const std::filesystem::path &path,
						SourceType source_type = SourceType::Unknown) noexcept
			: Tracebuffer(path, source_type) {};
	};

	struct SnapTracebuffer;
	using SnapTracebufferPtr = std::unique_ptr<SnapTracebuffer>;
	using SnapTracebufferCollection = std::vector<SnapTracebufferPtr>;
	struct EXPORT SnapTracebuffer : Tracebuffer {
		static SnapTracebufferCollection
		collect(const std::filesystem::path &, const TracebufferFilterFunc &tracebufferFilter = {},
				const TracepointFilterFunc &tracepointFilter = {});
		static SnapTracebufferPtr make(const std::filesystem::path &,
									   const TracepointFilterFunc &tracepointFilter = {});

		TracepointCollection tracepoints;

		const std::string_view name() const noexcept override { return m_name; }
		size_t size() const noexcept override { return m_size; }

		// is path, directory, clltk user or kernel trace or is compressed or uncompressed tar
		static bool is_formattable(const std::filesystem::path &);

		SnapTracebuffer(const SnapTracebuffer &) noexcept = default;
		SnapTracebuffer(SnapTracebuffer &&) noexcept = default;
		SnapTracebuffer &operator=(const SnapTracebuffer &) noexcept = delete;
		SnapTracebuffer &operator=(SnapTracebuffer &&) noexcept = delete;

		~SnapTracebuffer() noexcept override = default;

	  protected:
		SnapTracebuffer(const std::filesystem::path &, TracepointCollection &&, std::string &&,
						size_t, SourceType source_type = SourceType::Unknown) noexcept;
		const std::string m_name;
		const uint64_t m_size;
	};

} // namespace CommonLowLevelTracingKit::decoder

#endif