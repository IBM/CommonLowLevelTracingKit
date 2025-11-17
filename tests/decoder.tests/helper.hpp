#pragma once

#include <boost/cstdint.hpp>
#include <filesystem>
#include <functional>
#include <span>
#include <string_view>
#include <type_traits>

#include "CommonLowLevelTracingKit/tracing.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <ostream>
#include <string>

namespace std
{
template <typename T> struct hash<std::span<const T>> {
	std::size_t operator()(const std::span<const T> &p) const noexcept
	{
		static_assert(std::is_trivially_copyable_v<T>,
					  "T must be trivially copyable for hashing span<const T>");

		const auto *data = reinterpret_cast<const char *>(p.data());
		std::string_view sv{data, p.size_bytes()};
		return std::hash<std::string_view>{}(sv);
	}
};
} // namespace std

namespace detail
{
template <class CharT> std::basic_string<CharT> to_decimal_string(__uint128_t x)
{
	if (x == 0) return std::basic_string<CharT>(1, CharT('0'));

	std::basic_string<CharT> s;
	while (x > 0) {
		__uint128_t q = x / 10;
		const char digit = static_cast<char>(x - q * 10);
		s.push_back(CharT('0') + digit);
		x = q;
	}
	std::reverse(s.begin(), s.end());
	return s;
}
} // namespace detail

template <typename CharT, typename Traits>
inline std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &os,
													 __uint128_t x)
{
	auto s = detail::to_decimal_string<CharT>(x);
	return os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

[[maybe_unused]]
static std::filesystem::path trace_file(const std::string &name)
{
	const char *const env_path = std::getenv("CLLTK_TRACING_PATH");
	const std::string base_path_str = (env_path != nullptr) ? env_path : "./";
	const std::filesystem::path path = base_path_str / std::filesystem::path(name + ".clltk_trace");
	const std::filesystem::path absolute_path = std::filesystem::absolute(path);
	return absolute_path;
}
static pthread_mutex_t lock;
#define SETUP(TB_NAME) _SETUP(TB_NAME)
#define _SETUP(TB_NAME) _setup(_clltk_##TB_NAME)
static inline void _setup(_clltk_tracebuffer_handler_t &tb)
{
	pthread_mutex_lock(&lock);
	static const char dummy[] = "Hello World";
	tb.meta.start = (const void *)dummy;
	tb.meta.stop = (const void *)(dummy + sizeof(dummy));
	const auto file = trace_file(tb.definition.name);
	if (std::filesystem::exists(file)) std::filesystem::remove(file);
	ASSERT_FALSE(std::filesystem::exists(file));
	ASSERT_FALSE(tb.runtime.tracebuffer);
	const uint32_t meta_size = (uint32_t)((uint64_t)tb.meta.stop - (uint64_t)tb.meta.start);
	_clltk_tracebuffer_init(&tb);
	tb.runtime.file_offset = _clltk_tracebuffer_add_to_stack(&tb, tb.meta.start, meta_size);
	ASSERT_TRUE(tb.runtime.tracebuffer);
	ASSERT_TRUE(std::filesystem::exists(file));
}

#define CLEANUP(TB_NAME) _CLEANUP(TB_NAME)
#define _CLEANUP(TB_NAME) _cleanup(_clltk_##TB_NAME)
static inline void _cleanup(_clltk_tracebuffer_handler_t &tb)
{
	const auto file = trace_file(tb.definition.name);
	ASSERT_TRUE(std::filesystem::exists(file));
	ASSERT_TRUE(tb.runtime.tracebuffer);
	_clltk_tracebuffer_deinit(&tb);
	ASSERT_FALSE(tb.runtime.tracebuffer);
	pthread_mutex_unlock(&lock);
}
#ifndef STR
#define _STR(...) #__VA_ARGS__
#define STR(...) _STR(__VA_ARGS__)
#endif // !STR

#define TP(...) CLLTK_TRACEPOINT(TB, __VA_ARGS__)
#define DUMP(...) CLLTK_TRACEPOINT_DUMP(TB, __VA_ARGS__)

constexpr std::string operator+(const std::string &lhs, std::string_view rhs)
{
	std::string result(lhs);
	result.append(rhs);
	return result;
}
template <typename T> static constexpr const std::span<const T> span(const std::string_view &str)
{
	const T *const data = std::bit_cast<const T *>(str.data());
	const size_t size = str.size(); // without \0
	return std::span<const T>{data, size};
}