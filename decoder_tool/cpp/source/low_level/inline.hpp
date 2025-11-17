
#ifndef DECODER_TOOL_SOURCE_INLINE_HEADER
#define DECODER_TOOL_SOURCE_INLINE_HEADER

#include <cstdint>
#include <limits>
#include <type_traits>

#ifndef STR
	#define _STR_INTERNAL(...) #__VA_ARGS__
	#define STR(...) _STR_INTERNAL(__VA_ARGS__)
#endif

#if defined(__GNUC__) && !defined(__clang__)
	#define INLINE __attribute__((hot, always_inline, artificial, flatten)) inline
	#define CONST_INLINE __attribute__((const, hot, always_inline, artificial, flatten)) inline
	#define PRAGMA_COMPILER(...) _Pragma(STR(GCC __VA_ARGS__))
	#define PRAGMA(...) _Pragma(STR(__VA_ARGS__))
	#define PRAGMA_GCC(...) _Pragma(STR(__VA_ARGS__))
	#define PRAGMA_CLANG(...)
#elif defined(__clang__)
	#define INLINE __attribute__((hot, always_inline, artificial, flatten)) inline
	#define CONST_INLINE __attribute__((const, hot, always_inline, artificial, flatten)) inline
	#define PRAGMA_COMPILER(...) _Pragma(STR(clang __VA_ARGS__))
	#define PRAGMA(...) _Pragma(STR(__VA_ARGS__))
	#define PRAGMA_GCC(...)
	#define PRAGMA_CLANG(...) _Pragma(STR(__VA_ARGS__))
#else
	#define INLINE inline
	#define CONST_INLINE inline
	#define PRAGMA(...)
	#define PRAGMA_COMPILER(...)
	#define PRAGMA_GCC(...)
	#define PRAGMA_CLANG(...)
#endif

template <typename To, typename From> CONST_INLINE constexpr To safe_cast(From value) {
	static_assert(std::is_integral_v<From>, "From must be an integral type");
	static_assert(std::is_integral_v<To>, "To   must be an integral type");

	if constexpr (std::is_signed_v<From> && std::is_unsigned_v<To>) {
		// signed -> unsigned
		if (value < 0) { return To(0); }
		const auto uv = static_cast<std::make_unsigned_t<From>>(value);
		if (uv > std::numeric_limits<To>::max()) { return std::numeric_limits<To>::max(); }
		return static_cast<To>(uv);

	} else if constexpr (std::is_unsigned_v<From> && std::is_signed_v<To>) {
		// unsigned -> signed
		using UTo = std::make_unsigned_t<To>;
		if (value > static_cast<UTo>(std::numeric_limits<To>::max())) {
			return std::numeric_limits<To>::max();
		}
		return static_cast<To>(value);

	} else {
		// both signed or both unsigned
		using Common = std::common_type_t<From, To>;
		const Common v = static_cast<Common>(value);
		const Common lo = static_cast<Common>(std::numeric_limits<To>::min());
		const Common hi = static_cast<Common>(std::numeric_limits<To>::max());
		if (v < lo) return std::numeric_limits<To>::min();
		if (v > hi) return std::numeric_limits<To>::max();
		return static_cast<To>(v);
	}
}
static_assert(safe_cast<uint8_t>(0) == 0);
static_assert(safe_cast<uint8_t>(256) == 255);
static_assert(safe_cast<uint8_t>(UINT16_MAX) == 255);

static_assert(safe_cast<int8_t>(INT16_MIN) == -128);
static_assert(safe_cast<int8_t>(-129) == -128);
static_assert(safe_cast<int8_t>(-128) == -128);
static_assert(safe_cast<int8_t>(-1) == -1);
static_assert(safe_cast<int8_t>(0) == 0);
static_assert(safe_cast<int8_t>(127) == 127);
static_assert(safe_cast<int8_t>(128) == 127);
static_assert(safe_cast<int8_t>(INT16_MAX) == 127);
static_assert(safe_cast<int8_t>(UINT16_MAX) == 127);
#endif