#ifndef DECODER_TOOL_TRACEPOINT_HEADER
#define DECODER_TOOL_TRACEPOINT_HEADER
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common.hpp"

namespace CommonLowLevelTracingKit::decoder {

	/**
	 * @brief Source type for trace origin identification
	 */
	enum class SourceType : uint8_t {
		Unknown = 0x00,
		Userspace = 0x01,
		Kernel = 0x02,
		TTY = 0x03,
	};

	struct Tracepoint;

	/**
	 * @brief Deleter for Tracepoint that supports both heap and pool allocation
	 *
	 * When pool is nullptr, uses delete (heap allocation).
	 * When pool is set, returns memory to the pool.
	 * The deallocator is a type-erased function to avoid exposing pool types in public header.
	 */
	struct EXPORT TracepointDeleter {
		using DeallocFunc = void (*)(void *pool, void *ptr);

		constexpr TracepointDeleter() noexcept = default;
		constexpr TracepointDeleter(void *pool, DeallocFunc dealloc) noexcept
			: m_pool(pool)
			, m_dealloc(dealloc) {}

		void operator()(Tracepoint *ptr) const noexcept;

	  private:
		void *m_pool{nullptr};
		DeallocFunc m_dealloc{nullptr};
	};

	using TracepointPtr = std::unique_ptr<Tracepoint, TracepointDeleter>;
	using TracepointCollection = std::vector<TracepointPtr>;

	/**
	 * @brief Span information for Static tracepoints of span_begin / span_end type.
	 *
	 * Only present when a Tracepoint is a span event (Static tracepoint with MetaType
	 * span_begin or span_end).  Obtained via Tracepoint::span_info().
	 */
	struct EXPORT SpanInfo {
		enum class Kind { Begin, End };
		Kind kind;
		uint64_t id;
		uint64_t parent_id; ///< 0 when no parent (span_begin only; always 0 for span_end)
		std::string name;	///< span name (format string); empty for span_end
	};

	struct EXPORT Tracepoint {
		enum class Type {
			Dynamic = 1,
			Virtual = 2,
			Error = 2,
			Static = 0x101,
		};
		virtual ~Tracepoint() = default;

		const uint64_t nr;
		const uint64_t timestamp_ns;
		const SourceType source_type;
		virtual Type type() const noexcept = 0;
		[[nodiscard]] virtual const std::string_view tracebuffer() const noexcept = 0;
		[[nodiscard]] virtual const std::string_view file() const noexcept = 0;
		[[nodiscard]] virtual uint64_t line() const noexcept = 0;
		[[nodiscard]] virtual uint32_t pid() const noexcept = 0;
		[[nodiscard]] virtual uint32_t tid() const noexcept = 0;
		[[nodiscard]] virtual const std::string_view msg() const = 0;

		/**
		 * @brief Return span information if this is a span begin/end tracepoint.
		 *
		 * Returns nullopt for non-Static tracepoints and for Static tracepoints
		 * whose MetaType is printf or dump.
		 */
		[[nodiscard]] virtual std::optional<SpanInfo> span_info() const noexcept { return {}; }

		[[nodiscard]] std::string timestamp_str() const noexcept;
		[[nodiscard]] std::string date_and_time_str() const noexcept;

		[[nodiscard]] bool is_kernel() const noexcept {
			return source_type == SourceType::Kernel || source_type == SourceType::TTY;
		}

	  protected:
		Tracepoint(uint64_t n, uint64_t t, SourceType src = SourceType::Unknown) noexcept
			: nr(n)
			, timestamp_ns(t)
			, source_type(src) {};
	};

} // namespace CommonLowLevelTracingKit::decoder

#endif