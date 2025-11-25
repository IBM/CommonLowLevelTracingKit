#ifndef DECODER_TOOL_TRACEPOINT_HEADER
#define DECODER_TOOL_TRACEPOINT_HEADER
#include <memory>
#include <vector>

#include "Common.hpp"

namespace CommonLowLevelTracingKit::decoder {
	struct Tracepoint;
	using TracepointPtr = std::unique_ptr<Tracepoint>;
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
		virtual Type type() const noexcept = 0;
		[[nodiscard]] virtual const std::string_view file() const noexcept = 0;
		[[nodiscard]] virtual uint64_t line() const noexcept = 0;
		[[nodiscard]] virtual uint32_t pid() const noexcept = 0;
		[[nodiscard]] virtual uint32_t tid() const noexcept = 0;
		[[nodiscard]] virtual const std::string_view msg() const = 0;

		[[nodiscard]] std::string timestamp_str() const noexcept;
		[[nodiscard]] std::string date_and_time_str() const noexcept;

	  protected:
		Tracepoint(const std::string_view tb, uint64_t n, uint64_t t) noexcept
			: tracebuffer(tb)
			, nr(n)
			, timestamp_ns(t) {};
	};

} // namespace CommonLowLevelTracingKit::decoder

#endif