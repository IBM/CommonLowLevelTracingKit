#ifndef DECODER_TOOL_TRACEPOINT_HEADER
#define DECODER_TOOL_TRACEPOINT_HEADER
#include <cstdint>
#include <memory>
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
	struct EXPORT Tracepoint {
		enum class Type {
			Dynamic = 1,
			Virtual = 2,
			Error = 2,
			Static = 0x101,
		};
		virtual ~Tracepoint() = default;

		const std::string_view tracebuffer;
		const uint64_t nr;
		const uint64_t timestamp_ns;
		const SourceType source_type;
		virtual Type type() const noexcept = 0;
		[[nodiscard]] virtual const std::string_view file() const noexcept = 0;
		[[nodiscard]] virtual uint64_t line() const noexcept = 0;
		[[nodiscard]] virtual uint32_t pid() const noexcept = 0;
		[[nodiscard]] virtual uint32_t tid() const noexcept = 0;
		[[nodiscard]] virtual const std::string_view msg() const = 0;

		[[nodiscard]] std::string timestamp_str() const noexcept;
		[[nodiscard]] std::string date_and_time_str() const noexcept;

		[[nodiscard]] bool is_kernel() const noexcept {
			return source_type == SourceType::Kernel || source_type == SourceType::TTY;
		}

	  protected:
		Tracepoint(const std::string_view tb, uint64_t n, uint64_t t,
				   SourceType src = SourceType::Unknown) noexcept
			: tracebuffer(tb)
			, nr(n)
			, timestamp_ns(t)
			, source_type(src) {};
	};

} // namespace CommonLowLevelTracingKit::decoder

#endif