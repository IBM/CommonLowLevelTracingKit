
#ifndef DECODER_TOOL_SOURCE_INLINE_HEADER
#define DECODER_TOOL_SOURCE_INLINE_HEADER

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

#if defined(__GNUC__)
	#define INLINE __attribute__((hot, always_inline, artificial, flatten)) inline
	#define CONST_INLINE __attribute__((const, hot, always_inline, artificial, flatten)) inline
#elif defined(__clang__)
	#define INLINE __attribute__((hot, always_inline, artificial, flatten)) inline
	#define CONST_INLINE __attribute__((const, hot, always_inline, artificial, flatten)) inline
#else
	#define INLINE inline
	#define CONST_INLINE inline
#endif

template <typename To, typename From> CONST_INLINE constexpr To safe_narrow_cast(From value) {
	static_assert(std::is_integral<From>::value, "From must be an integral type");
	static_assert(std::is_integral<To>::value, "To must be an integral type");

	if (value < static_cast<From>(std::numeric_limits<To>::min())) {
		return std::numeric_limits<To>::min();
	} else if (value > static_cast<From>(std::numeric_limits<To>::max())) {
		return std::numeric_limits<To>::max();
	}
	return static_cast<To>(value);
}

#endif