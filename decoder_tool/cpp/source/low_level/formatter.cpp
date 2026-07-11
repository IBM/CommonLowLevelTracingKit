#include "formatter.hpp"

#include "CommonLowLevelTracingKit/decoder/Common.hpp"
#include "file.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstring>
#include <ffi.h>
#include <format>
#include <stdio.h>
#include <variant>

#include "CommonLowLevelTracingKit/decoder/Inline.hpp"

namespace formatter = CommonLowLevelTracingKit::decoder::source::formatter;
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
	default: CLLTK_DECODER_THROW(FormattingFailed, "unknown type");
	}
}
CONST_INLINE static constexpr size_t clltk_arg_to_size(const char clltk_type,
													   const uintptr_t clltk_arg, size_t remaining,
													   bool foreign_endian) {
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
		if (foreign_endian) {
			size = CommonLowLevelTracingKit::decoder::source::internal::byteswapValue(size);
		}
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
	default: CLLTK_DECODER_THROW(FormattingFailed, "unknown type");
	}
}
template <typename T, typename ProxyT = T>
static INLINE const any get_native(uintptr_t p, size_t remaining, bool foreign_endian) {
	if (sizeof(T) > remaining) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "out of range access for formatter");
	T value{};
	memcpy(&value, std::bit_cast<void *>(p), sizeof(T));
	if constexpr (CommonLowLevelTracingKit::decoder::source::internal::ByteSwappable<T>) {
		if (foreign_endian) {
			value = CommonLowLevelTracingKit::decoder::source::internal::byteswapValue(value);
		}
	}
	static_assert(sizeof(ProxyT) == sizeof(void *));
	ProxyT xvalue = (ProxyT)value;
	return xvalue;
}
INLINE static constexpr any clltk_arg_to_native(const char clltk_type, const uintptr_t clltk_arg,
												size_t remaining, bool foreign_endian) {
	switch (clltk_type) {
	case 'c': return get_native<uint8_t, uint64_t>(clltk_arg, remaining, foreign_endian);
	case 'C': return get_native<int8_t, int64_t>(clltk_arg, remaining, foreign_endian);
	case 'w': return get_native<uint16_t, uint64_t>(clltk_arg, remaining, foreign_endian);
	case 'W': return get_native<int16_t, int64_t>(clltk_arg, remaining, foreign_endian);
	case 'i': return get_native<uint32_t, uint64_t>(clltk_arg, remaining, foreign_endian);
	case 'I': return get_native<int32_t, int64_t>(clltk_arg, remaining, foreign_endian);
	case 'l': return get_native<uint64_t>(clltk_arg, remaining, foreign_endian);
	case 'L': return get_native<int64_t>(clltk_arg, remaining, foreign_endian);
	case 'f':
		// use double as a proxy type to get valid data from raw args
		return get_native<float, double>(clltk_arg, remaining, foreign_endian);
	case 'd': return get_native<double>(clltk_arg, remaining, foreign_endian);
	case 'p': return get_native<uint64_t>(clltk_arg, remaining, foreign_endian);
	case 's': return std::bit_cast<uint64_t>(clltk_arg + sizeof(uint32_t));
	case InvalidStringArgType:
		// the value in clltk_arg is a pointer to a now invalid/unusable memory address.
		// replace it with a dummy arg string
		return std::bit_cast<uint64_t>(&InvalidStringArg);
	default: CLLTK_DECODER_THROW(FormattingFailed, "unknown type");
	}
}

INLINE static std::array<any, total_arg_count>
clltk_args_to_native_args(const std::string_view format, const std::span<const char> &clltk_types,
						  const std::span<const uint8_t> &raw_clltk_args, bool foreign_endian) {
	std::array<any, total_arg_count> args{};
	args[0] = std::bit_cast<uint64_t>(nullptr);
	args[1] = std::bit_cast<uint64_t>(0lu);
	args[2] = const_cast<char *>(format.data());
	size_t raw_arg_offset = 0;
	for (size_t i = 0; i < clltk_types.size(); i++) {
		const char type = clltk_types[i];
		if (raw_arg_offset >= raw_clltk_args.size()) [[unlikely]]
			CLLTK_DECODER_THROW(FormattingFailed, "out of range access for formatter");
		const uintptr_t current = std::bit_cast<uintptr_t>(&raw_clltk_args[raw_arg_offset]);
		const size_t remaining = raw_clltk_args.size() - raw_arg_offset;
		const any value = clltk_arg_to_native(type, current, remaining, foreign_endian);
		args[fix_arg_count + i] = value;
		const size_t arg_size = clltk_arg_to_size(type, current, remaining, foreign_endian);
		raw_arg_offset += arg_size;
	}

	if (raw_arg_offset != raw_clltk_args.size()) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "raw args invalid");
	return args;
}

// the macros for detection the arg type could not different between a char* as a pointer or as a
// string to get the correct type we need to search for the format specifier.
INLINE static auto fix_types_based_on_format(const std::string_view format,
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
				if (arg_count >= raw_types.size()) [[unlikely]] {
					// Check before accessing raw_types[arg_count] to prevent out-of-bounds access
					CLLTK_DECODER_THROW(FormattingFailed,
										"format specifier count exceeds argument count");
				}
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
		CLLTK_DECODER_THROW(FormattingFailed, "format specifier count mismatch");
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

	// ISA-independent optimized path
	// Process 8 bytes at a time for better performance
	size_t i = 0;
	const size_t unroll_factor = 8;

	// Main loop - process 8 characters at a time
	for (; i + unroll_factor <= n; i += unroll_factor) {
		// Manual unrolling for better performance
		if (ptr[i + 0] < 32) ptr[i + 0] = ' ';
		if (ptr[i + 1] < 32) ptr[i + 1] = ' ';
		if (ptr[i + 2] < 32) ptr[i + 2] = ' ';
		if (ptr[i + 3] < 32) ptr[i + 3] = ' ';
		if (ptr[i + 4] < 32) ptr[i + 4] = ' ';
		if (ptr[i + 5] < 32) ptr[i + 5] = ' ';
		if (ptr[i + 6] < 32) ptr[i + 6] = ' ';
		if (ptr[i + 7] < 32) ptr[i + 7] = ' ';
	}

	// Handle remainder
	for (; i < n; ++i) {
		if (ptr[i] < 32) ptr[i] = ' ';
	}
}

static INLINE std::string clean_up_str_view(const std::string_view str) {
	std::string msg{str};
	clean_up_str(msg);
	while (msg.size() && msg.back() == '\0') [[unlikely]]
		msg.resize(msg.size() - 1);
	return msg;
}

// call snprintf with ffi
std::string formatter::printf(const std::string_view format, const std::span<const char> &types_raw,
							  const std::span<const uint8_t> &args_raw, bool foreign_endian) {
	if (format.empty()) return "";
	if (format.data()[format.size()] != '\0')
		CLLTK_DECODER_THROW(FormattingFailed, "missing format termination");
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
	auto arg_values = clltk_args_to_native_args(format, fixed_types, args_raw, foreign_endian);

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
	// IMPORTANT: FFI (Foreign Function Interface) is necessary here and cannot be replaced
	// with direct snprintf calls for the following reasons:
	//
	// 1. Type Safety: snprintf is a variadic function that requires arguments to be passed
	//    with their correct types. We cannot pass all arguments as uint64_t or void*.
	//    Example: snprintf(buf, size, "%s %d %f", str, num, flt) requires:
	//    - str as char* (not uint64_t)
	//    - num as int (not void*)
	//    - flt as double (not uint64_t)
	//
	// 2. Runtime Argument Count: We have 0-10 arguments determined at runtime. C++ has no
	//    clean way to call variadic functions with runtime-determined argument counts.
	//    A switch with all combinations would need 11 cases × multiple type combinations.
	//
	// 3. Type Mixing: Arguments can be any combination of integers, floats, doubles, and
	//    strings. Handling all possible type combinations for up to 10 arguments would
	//    require an impractical number of switch cases.
	//
	// FFI solves this by dynamically constructing the function call with the correct
	// types at runtime, which is exactly what we need for this use case.
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

// Variant type that can hold any decoded argument value for std::vformat.
// std::make_format_args requires lvalues so we store into a variant array first.
using fmt_arg_value = std::variant<uint64_t, int64_t, double, std::string>;

// Read a POD value of type T from raw memory at clltk_arg, byte-swapping if needed.
template <typename T>
INLINE static T read_raw(uintptr_t clltk_arg, size_t remaining, bool foreign_endian) {
	if (sizeof(T) > remaining) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "out of range access for fmt formatter");
	T value{};
	memcpy(&value, reinterpret_cast<const void *>(clltk_arg), sizeof(T));
	if constexpr (CommonLowLevelTracingKit::decoder::source::internal::ByteSwappable<T>) {
		if (foreign_endian)
			value = CommonLowLevelTracingKit::decoder::source::internal::byteswapValue(value);
	}
	return value;
}

INLINE static fmt_arg_value clltk_arg_to_fmt_value(const char clltk_type, const uintptr_t clltk_arg,
												   size_t remaining, bool foreign_endian) {
	switch (clltk_type) {
	case 'c': return static_cast<uint64_t>(read_raw<uint8_t>(clltk_arg, remaining, foreign_endian));
	case 'w':
		return static_cast<uint64_t>(read_raw<uint16_t>(clltk_arg, remaining, foreign_endian));
	case 'i':
		return static_cast<uint64_t>(read_raw<uint32_t>(clltk_arg, remaining, foreign_endian));
	case 'l':
		return static_cast<uint64_t>(read_raw<uint64_t>(clltk_arg, remaining, foreign_endian));
	case 'C': return static_cast<int64_t>(read_raw<int8_t>(clltk_arg, remaining, foreign_endian));
	case 'W': return static_cast<int64_t>(read_raw<int16_t>(clltk_arg, remaining, foreign_endian));
	case 'I': return static_cast<int64_t>(read_raw<int32_t>(clltk_arg, remaining, foreign_endian));
	case 'L': return static_cast<int64_t>(read_raw<int64_t>(clltk_arg, remaining, foreign_endian));
	case 'f': return static_cast<double>(read_raw<float>(clltk_arg, remaining, foreign_endian));
	case 'd': return static_cast<double>(read_raw<double>(clltk_arg, remaining, foreign_endian));
	case 'p':
		return static_cast<uint64_t>(read_raw<uint64_t>(clltk_arg, remaining, foreign_endian));
	case 's': {
		// string: uint32 length prefix then null-terminated chars; skip the length prefix
		return std::string{reinterpret_cast<const char *>(clltk_arg + sizeof(uint32_t))};
	}
	default: CLLTK_DECODER_THROW(FormattingFailed, "unknown type for fmt");
	}
}

// Wrapper so one std::formatter specialization can format any decoded value.
// Dispatching the concrete types at the make_format_args call site would need
// nested std::visit over all argument slots, instantiating vformat for every
// type combination (4^10). Instead the wrapper's formatter captures the format
// spec during parse() and applies it to the variant's active alternative with
// a single std::visit per argument.
struct FmtDecodedArg {
	fmt_arg_value value{};
};

template <> struct std::formatter<FmtDecodedArg, char> {
	std::string spec;

	constexpr auto parse(std::format_parse_context &ctx) {
		auto it = ctx.begin();
		const auto end = ctx.end();
		while (it != end && *it != '}') { spec += *it++; }
		return it;
	}

	auto format(const FmtDecodedArg &arg, std::format_context &ctx) const {
		const std::string one_arg_format = "{:" + spec + "}";
		return std::visit(
			[&](const auto &value) {
				return std::vformat_to(ctx.out(), one_arg_format, std::make_format_args(value));
			},
			arg.value);
	}
};

std::string formatter::fmt(const std::string_view format, const std::span<const char> &types_raw,
						   const std::span<const uint8_t> &args_raw, bool foreign_endian) {
	if (format.empty()) return "";

	// Decode all arguments into a variant array so we have lvalues for make_format_args.
	static constexpr size_t max_fmt_args = 10;
	std::array<FmtDecodedArg, max_fmt_args> decoded{};
	const size_t arg_count = types_raw.size();
	if (arg_count > max_fmt_args) CLLTK_DECODER_THROW(FormattingFailed, "too many fmt args");

	size_t raw_offset = 0;
	for (size_t i = 0; i < arg_count; ++i) {
		const char type = types_raw[i];
		if (raw_offset >= args_raw.size())
			CLLTK_DECODER_THROW(FormattingFailed, "out of range access for fmt formatter");
		const uintptr_t current = std::bit_cast<uintptr_t>(&args_raw[raw_offset]);
		const size_t remaining = args_raw.size() - raw_offset;
		// Validate and compute size first (clltk_arg_to_size checks string bounds).
		const size_t arg_size = clltk_arg_to_size(type, current, remaining, foreign_endian);
		decoded[i].value = clltk_arg_to_fmt_value(type, current, remaining, foreign_endian);
		raw_offset += arg_size;
	}
	if (raw_offset != args_raw.size()) [[unlikely]]
		CLLTK_DECODER_THROW(FormattingFailed, "raw args invalid");

	// Pass all slots; std::vformat ignores arguments the format string does not
	// reference, so the unused trailing (default-constructed) slots are harmless.
	try {
		return std::vformat(format,
							std::make_format_args(decoded[0], decoded[1], decoded[2], decoded[3],
												  decoded[4], decoded[5], decoded[6], decoded[7],
												  decoded[8], decoded[9]));
	} catch (const std::format_error &) { return std::string(format) + " fmt-error"; }
}

std::string formatter::dump(const std::string_view format, const std::span<const char> &types_raw,
							const std::span<const uint8_t> &args_raw, bool foreign_endian) {
	if (types_raw.size() != 1 || types_raw[0] != 'x')
		CLLTK_DECODER_THROW(InvalidMeta, "wrong meta for drump tracepoint");
	const size_t format_size = format.size();
	if (args_raw.size() < sizeof(uint32_t))
		CLLTK_DECODER_THROW(FormattingFailed, "args_raw too small for dump size");
	uint32_t dump_size;
	std::memcpy(&dump_size, args_raw.data(), sizeof(dump_size));
	if (foreign_endian) {
		dump_size = CommonLowLevelTracingKit::decoder::source::internal::byteswapValue(dump_size);
	}
	if (args_raw.size() < sizeof(uint32_t) + dump_size)
		CLLTK_DECODER_THROW(FormattingFailed, "args_raw too small for dump body");
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