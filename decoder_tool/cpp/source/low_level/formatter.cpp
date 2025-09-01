#include "formatter.hpp"

#include "CommonLowLevelTracingKit/Decoder/Common.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <ffi.h>
#include <stdio.h>
#include <variant>

#include "inline.hpp"

namespace formater = CommonLowLevelTracingKit::decoder::source::formatter;
using namespace CommonLowLevelTracingKit::decoder::exception;
using namespace std::string_literals;

using any = std::variant<uint64_t, int64_t, double, char *>;

static constexpr size_t fix_arg_count = 3;
static constexpr size_t max_arg_count = 10;
static constexpr size_t total_arg_count = fix_arg_count + max_arg_count;
static constexpr char InvalidStringArgType = 'Z';
static constexpr char InvalidStringArg[] = "<invalid arg>";

CONST_INLINE static constexpr ffi_type *clltk_type_to_ffi_type(const char clltk_type) {
	switch (clltk_type) {
	case 'c': return &ffi_type_uint64;
	case 'C': return &ffi_type_sint64;
	case 'w': return &ffi_type_uint64;
	case 'W': return &ffi_type_sint64;
	case 'i': return &ffi_type_uint64;
	case 'I': return &ffi_type_sint64;
	case 'l': return &ffi_type_uint64;
	case 'L': return &ffi_type_sint64;
	case 'f': return &ffi_type_double; // snprintf only works with floads used as doubles
	case 'd': return &ffi_type_double;
	case 's': return &ffi_type_pointer;
	case 'p': return &ffi_type_pointer;
	case InvalidStringArgType: return &ffi_type_pointer;
	default: CLLTK_DECODER_THROW(FormattingFailed, "unkown type");
	}
}
CONST_INLINE static constexpr size_t
clltk_arg_to_size(const char clltk_type, const uintptr_t clltk_arg, size_t remaining) {
	switch (clltk_type) {
	case 'c': return sizeof(uint8_t);
	case 'C': return sizeof(int8_t);
	case 'w': return sizeof(uint16_t);
	case 'W': return sizeof(int16_t);
	case 'i': return sizeof(uint32_t);
	case 'I': return sizeof(int32_t);
	case 'l': return sizeof(uint64_t);
	case 'L': return sizeof(int64_t);
	case 'f': return sizeof(float);
	case 'd': return sizeof(double);
	case 's': {
		uint32_t size{};
		if (sizeof(size) > remaining) [[unlikely]]
			CLLTK_DECODER_THROW(FormattingFailed, "no space for string arg size left");
		memcpy(&size, std::bit_cast<void *>(clltk_arg), sizeof(size));
		size += sizeof(uint32_t);
		if (size > remaining) [[unlikely]]
			CLLTK_DECODER_THROW(FormattingFailed, "string arg bigger than raw args");
		char last_char{};
		memcpy(&last_char, std::bit_cast<void *>(clltk_arg + size - 1), sizeof(last_char));
		if (last_char != '\0') [[unlikely]]
			CLLTK_DECODER_THROW(FormattingFailed, "missing string arg termination");
		return size;
	}
	case 'p': return sizeof(void *);
	case InvalidStringArgType: return sizeof(void *);
	default: CLLTK_DECODER_THROW(FormattingFailed, "unkown type");
	}
}
template <typename T, typename ProxyT = T>
static INLINE const any get_native(uintptr_t p, size_t remaining) {
	if (sizeof(T) > remaining) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "out of range access for formater");
	T value{};
	memcpy(&value, std::bit_cast<void *>(p), sizeof(T));
	static_assert(sizeof(ProxyT) == sizeof(void *));
	ProxyT xvalue = (ProxyT)value;
	return xvalue;
}
INLINE static constexpr any clltk_arg_to_native(const char clltk_type, const uintptr_t clltk_arg,
												size_t remaining) {
	switch (clltk_type) {
	case 'c': return get_native<uint8_t, uint64_t>(clltk_arg, remaining);
	case 'C': return get_native<int8_t, int64_t>(clltk_arg, remaining);
	case 'w': return get_native<uint16_t, uint64_t>(clltk_arg, remaining);
	case 'W': return get_native<int16_t, int64_t>(clltk_arg, remaining);
	case 'i': return get_native<uint32_t, uint64_t>(clltk_arg, remaining);
	case 'I': return get_native<int32_t, int64_t>(clltk_arg, remaining);
	case 'l': return get_native<uint64_t>(clltk_arg, remaining);
	case 'L': return get_native<int64_t>(clltk_arg, remaining);
	case 'f':
		return get_native<float, double>(
			clltk_arg, remaining); // use double as a proxy type to get valid data from raw args
	case 'd': return get_native<double>(clltk_arg, remaining);
	case 'p': return get_native<uint64_t>(clltk_arg, remaining);
	case 's': return reinterpret_cast<uint64_t>(clltk_arg + sizeof(uint32_t));
	case InvalidStringArgType:
		// the value in clltk_arg is a pointer to a now invalid/unusable memory address.
		// replace it with a dummy arg string
		return reinterpret_cast<uint64_t>(&InvalidStringArg);
	default: CLLTK_DECODER_THROW(FormattingFailed, "unkown type");
	}
}

INLINE static std::array<any, total_arg_count>
clltk_args_to_native_args(const std::string_view &format, const std::span<const char> &clltk_types,
						  const std::span<const uint8_t> &raw_clltk_args) {
	std::array<any, total_arg_count> args{};
	args[0] = reinterpret_cast<uint64_t>(nullptr);
	args[1] = reinterpret_cast<uint64_t>(0lu);
	args[2] = const_cast<char *>(format.data());
	size_t raw_arg_offset = 0;
	for (size_t i = 0; i < clltk_types.size(); i++) {
		const char type = clltk_types[i];
		const uintptr_t current = std::bit_cast<uintptr_t>(&raw_clltk_args[raw_arg_offset]);
		if (raw_arg_offset >= raw_clltk_args.size()) [[unlikely]]
			CLLTK_DECODER_THROW(FormattingFailed, "out of range access for formater");
		const size_t remaining = raw_clltk_args.size() - raw_arg_offset;
		const any value = clltk_arg_to_native(type, current, remaining);
		args[fix_arg_count + i] = value;
		const size_t arg_size = clltk_arg_to_size(type, current, remaining);
		raw_arg_offset += arg_size;
	}

	if (raw_arg_offset != raw_clltk_args.size()) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "raw args invalid");
	return args;
}

// the macros for detection the arg type could not different between a char* as a pointer or as a
// string to get the correct type we need to search for the format specifier.
INLINE static auto fix_types_based_on_format(const std::string_view &format,
											 const std::span<const char> &raw_types) {
	// check if any last char for a format specifier
	static constexpr auto is_final_char = [](const char c) {
		return (c == 'c') || (c == 'd') || (c == 'u') || (c == 'x') || (c == 'X') || (c == 'e') ||
			   (c == 'E') || (c == 'f') || (c == 'g') || (c == 'G') || (c == 's') || (c == 'p') ||
			   (c == 'o') || (c == 'i');
	};
	const char *const f = format.data();
	const size_t f_size = format.size();
	std::array<char, max_arg_count + 1> out{};
	std::copy(raw_types.begin(), raw_types.end(), out.begin());

	enum : uint8_t { format_specifier, other } parse_state = other;
	size_t arg_count = 0;
	for (size_t char_offset = 0; char_offset < f_size; char_offset++) {
		const char c = f[char_offset];
		switch (parse_state) {
		[[likely]] case other: // everything not related to format specifier
			if (c == '%') { parse_state = format_specifier; }
			break;
		[[unlikely]] case format_specifier: // everything related to format specifier
			if (c == '%') [[unlikely]] {
				parse_state = other;
			} else if (is_final_char(c)) { // end of format specifier
				char type = raw_types[arg_count];
				if (c == 'p' && type == 's') [[unlikely]] {
					// this case is handled by the tracing_library
					type = 'p';
				} else if (c == 's' && type == 'p') [[unlikely]] {
					// this is not handled by the tracing_library and needs to be handled here
					// to prefend invalid memory access by print
					type = InvalidStringArgType;
				} else if ((c == 's' && type != 's') || (c != 's' && type == 's')) [[unlikely]] {
					CLLTK_DECODER_THROW(FormattingFailed, "invalid format specifier");
				}
				out[arg_count++] = type;
				parse_state = other;
			}
			break;
		}
	}
	if (arg_count != raw_types.size()) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "invalid format secifier");
	return out;
}

INLINE static constexpr std::array<ffi_type *, total_arg_count>
clltk_types_to_ffi_types(const std::span<const char> &clltk_types) {
	std::array<ffi_type *, total_arg_count> types{};
	types[0] = &ffi_type_pointer; // char* (buffer)
	types[1] = &ffi_type_uint64;  // uint64 buffer size
	types[2] = &ffi_type_pointer; // char* (format)

	PRAGMA_GCC(GCC unroll max_arg_count)
	PRAGMA_GCC(GCC ivdep)
	PRAGMA_CLANG(unroll)
	PRAGMA_CLANG(clang loop interleave(disable))
	for (size_t i = 0; i < clltk_types.size(); i++) {
		types[fix_arg_count + i] = clltk_type_to_ffi_type(clltk_types[i]);
	}
	return types;
}

static INLINE void clean_up_str(std::string &s) {
	// Drop any trailing control chars (so we don't need look-ahead)
	while (!s.empty() && std::bit_cast<uint8_t>(s.back()) < 32) { s.pop_back(); }

	auto ptr = std::bit_cast<char *>(s.data());
	size_t n = s.size();

	PRAGMA_GCC(GCC ivdep)
	PRAGMA_CLANG(unroll)
	PRAGMA_CLANG(clang loop interleave(disable))
	for (size_t i = 0; i < n; ++i) {
		if (ptr[i] < 32) ptr[i] = ' ';
	}
}

static INLINE std::string clean_up_str_view(const std::string_view &str) {
	std::string msg{str};
	clean_up_str(msg);
	while (msg.size() && msg.back() == '\0') [[unlikely]]
		msg.resize(msg.size() - 1);
	return msg;
}

// call snprintf with ffi
std::string formater::printf(const std::string_view &format, const std::span<const char> &types_raw,
							 const std::span<const uint8_t> &args_raw) {
	if (format.empty()) return "";
	if (*format.end() != '\0') CLLTK_DECODER_THROW(FormattingFailed, "missing format termination");
	const auto fixed_typ_array = fix_types_based_on_format(format, types_raw);
	if (args_raw.empty()) return clean_up_str_view(format);
	const std::span<const char> fixed_types{fixed_typ_array.data(), types_raw.size()};
	auto arg_types = clltk_types_to_ffi_types(fixed_types);
	ffi_cif cif;
	if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
					 safe_cast<unsigned int>(fix_arg_count + fixed_types.size()), &ffi_type_uint,
					 arg_types.data()) != FFI_OK) [[unlikely]] {
		return "ffi_prep_cif failed";
	}
	auto arg_values = clltk_args_to_native_args(format, fixed_types, args_raw);

	// educated guess about the output size, used for first snprintf try
	const size_t guessed_size = format.size() + fixed_types.size() * 8 + 1;
	std::string msg{};
	msg.resize(guessed_size);

	arg_values[0] = msg.data();
	arg_values[1] = msg.size();
	void *values[total_arg_count] = {};

	PRAGMA_GCC(GCC unroll max_arg_count)
	PRAGMA_GCC(GCC ivdep)
	PRAGMA_CLANG(unroll)
	PRAGMA_CLANG(clang loop vectorize(enable))
	PRAGMA_CLANG(clang loop interleave(disable))
	for (size_t i = 0; i < total_arg_count; i++) values[i] = &arg_values[i];
	int rc = 0;
	ffi_call(&cif, (void (*)(void))snprintf, &rc, values); // first snprintf try
	if (rc < 0) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "first printf try failed");
	else if (rc == 0)
		return "";
	else if (safe_cast<size_t>(rc) >= guessed_size)
		[[unlikely]] { // guessed_size was not big enough
		const size_t string_size = safe_cast<size_t>(rc) + 1;
		msg.resize(string_size);
		arg_values[0] = msg.data();
		arg_values[1] = msg.size();

		PRAGMA_GCC(GCC unroll max_arg_count)
		PRAGMA_GCC(GCC ivdep)
		PRAGMA_CLANG(unroll)
		PRAGMA_CLANG(clang loop vectorize(enable))
		PRAGMA_CLANG(clang loop interleave(disable))
		for (size_t i = 0; i < total_arg_count; i++) values[i] = &arg_values[i];
		ffi_call(&cif, (void (*)(void))snprintf, &rc, values); // second/final snprintf
		if (rc < 0) [[unlikely]]
			CLLTK_DECODER_THROW(FormattingFailed, "second printf try failed");
		else if (rc == 0)
			return "";
	}
	msg.resize(safe_cast<size_t>(rc));
	clean_up_str(msg);
	return msg;
}

std::string formater::dump(const std::string_view &format, const std::span<const char> &types_raw,
						   const std::span<const uint8_t> &args_raw) {
	if (types_raw.size() != 1 || types_raw[0] != 'x')
		CLLTK_DECODER_THROW(InvalidMeta, "wrong meta for drump tracepoint");
	const size_t format_size = format.size();
	const uint32_t dump_size = (*reinterpret_cast<const uint32_t *>(args_raw.data()));
	const std::span<const uint8_t> dump_body{&args_raw[sizeof(uint32_t)], dump_size};
	static constexpr std::string_view dump_token{" =(dump)= "};
	const size_t output_size = format_size + dump_token.size() //
							   + (dump_size * 3)			   // for " XX" for each byte
							   + 1; // for starting ", closing " replaces a space
	std::string output{};
	output.resize(output_size);
	const int rc =
		snprintf(output.data(), output.size(), //
				 "%*s%s\"", safe_cast<int>(format.size()), format.data(), dump_token.data());
	if (rc < 0) CLLTK_DECODER_THROW(FormattingFailed, "printf try failed for dump");

	const size_t dump_byte_start_offset = safe_cast<size_t>(rc);
	for (size_t byte_offset = 0; byte_offset < dump_size; byte_offset++) {
		const uint8_t dump_char = dump_body[byte_offset];
		const size_t current_offset = dump_byte_start_offset + (byte_offset * 3);
		char *const current_output = &output[current_offset];
		snprintf(current_output, 4, "%02X ", 0xFF & dump_char);
	}
	const size_t termination_offset = dump_byte_start_offset + (dump_size * 3);
	output[termination_offset - 1] = '"';
	output[termination_offset] = '\0';
	clean_up_str(output);
	return output;
}