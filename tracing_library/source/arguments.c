// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "arguments.h"

#if defined(__KERNEL__)
#include <linux/limits.h>
#include <linux/string.h>
#include <linux/types.h>
#define UINT64_MAX U64_MAX
#define UINT32_MAX U32_MAX
#else
#include <stdbool.h>
#include <string.h>

static size_t strnlen_s(const char *str, size_t len)
{
	return (str != NULL) ? strnlen(str, len) : 0;
}
#endif

static inline bool is_final_char(const char c)
{
	return (c == 'c') || (c == 'd') || (c == 'u') || (c == 'x') || (c == 'X') || (c == 'e') ||
		   (c == 'E') || (c == 'f') || (c == 'g') || (c == 'G') || (c == 's') || (c == 'p') ||
		   (c == 'o') || (c == 'i');
}

void first_time_check(const char *const format, _clltk_argument_types_t *types)
{
	if (types->flex_size) {
		const size_t format_length = strnlen_s(format, 4094);
		enum { format_specifier, other } parse_state = other;
		size_t arg_count = 0;
		for (size_t char_offset = 0; char_offset < format_length; char_offset++) {
			const char c = format[char_offset];
			switch (parse_state) {
			case other: // everything not related to format specifier
				if (c == '%') {
					parse_state = format_specifier;
				}
				break;
			case format_specifier: // everything related to format specifier
				if (c == '%') {
					parse_state = other;
				} else if (is_final_char(c)) { // end of format specifier
					_clltk_argument_t *const type = &types->types[arg_count];
					if (c == 'p' && *type == _clltk_argument_t_string) {
						*type = _clltk_argument_t_pointer;
					}
					parse_state = other;
					arg_count++;
				}
				break;
			}
		}
		// check if still flex type
		bool any_flex_type = false;
		for (size_t arg_index = 0; arg_index < types->count; arg_index++)
			any_flex_type |= (types->types[arg_index] == _clltk_argument_t_string);
		types->flex_size = any_flex_type;
	}

	types->already_checked = true;
}

uint32_t get_argument_sizes(const char *const format, uint32_t sizes_out[],
							_clltk_argument_types_t *types, va_list args)
{
	if ((types == NULL) || (types->count == 0))
		return 0;

	if (!types->already_checked)
		first_time_check(format, types);

	uint32_t size = 0;
	if (!types->flex_size) {
		// everything is fixed so just use this values
		for (size_t arg_index = 0; arg_index < types->count; arg_index++) {
			const _clltk_argument_t type = types->types[arg_index];
			uint32_t fix_arg_size = _clltk_type_to_size(type);
			if (sizes_out != NULL)
				sizes_out[arg_index] = fix_arg_size;
			size += fix_arg_size;
		}
	} else {
		// get sizes of strings, therefor access va_list
		va_list args_copy;
		va_copy(args_copy, args);
		for (size_t arg_index = 0; arg_index < types->count; arg_index++) {
			const _clltk_argument_t type = types->types[arg_index];
			uint32_t fix_arg_size = _clltk_type_to_size(type);

			if (sizes_out != NULL)
				sizes_out[arg_index] = fix_arg_size;
			size += fix_arg_size;
			switch (type) {
			case _clltk_argument_t_uint8:
			case _clltk_argument_t_int8:
			case _clltk_argument_t_uint16:
			case _clltk_argument_t_int16:
			case _clltk_argument_t_uint32:
			case _clltk_argument_t_int32: {
				const uint32_t value = va_arg(args_copy, __typeof__(value));
				(void)value;
			} break;
			case _clltk_argument_t_uint64:
			case _clltk_argument_t_int64:
			case _clltk_argument_t_pointer: {
				const uint64_t value = va_arg(args_copy, __typeof__(value));
				(void)value;
			} break;
			case _clltk_argument_t_uint128:
			case _clltk_argument_t_int128: {
				const __uint128_t value = va_arg(args_copy, __typeof__(value));
				(void)value;
			} break;
			case _clltk_argument_t_float:
			case _clltk_argument_t_double:
#if !defined(__KERNEL__) // TODO create an error?
			{
				const double value = va_arg(args_copy, __typeof__(value));
				(void)value;
			}
#endif
			break;
			case _clltk_argument_t_string: {
				const char *const value = va_arg(args_copy, __typeof__(value));
				const uint32_t string_size =
					((value) ? (uint32_t)strnlen_s(value, UINT32_MAX) : 0) + 1; // + 1 for \0
				if (sizes_out != NULL)
					sizes_out[arg_index] += string_size;
				size += string_size;
			} break;
			case _clltk_argument_t_unknown:
			default:
				break;
			}
		}
		va_end(args_copy);
	}
	return size;
}

void get_arguments(void *_buffer, uint32_t sizes[], const _clltk_argument_types_t *types,
				   va_list args)
{
	uint64_t buffer = (uint64_t)_buffer;
	va_list args_copy;
	va_copy(args_copy, args);
	for (size_t arg_index = 0; arg_index < types->count; arg_index++) {
		const _clltk_argument_t type = types->types[arg_index];
		size_t fix_arg_size = _clltk_type_to_size(type);
		switch (type) {
		case _clltk_argument_t_uint8:
		case _clltk_argument_t_int8: {
			uint8_t value = (uint8_t)va_arg(args_copy, __typeof__(uint32_t));
			*(uint8_t *)buffer = value;
			buffer += fix_arg_size;
		} break;
		case _clltk_argument_t_uint16:
		case _clltk_argument_t_int16: {
			const uint16_t value = (uint16_t)va_arg(args_copy, __typeof__(uint32_t));
			*(uint16_t *)buffer = value;
			buffer += fix_arg_size;
		} break;
		case _clltk_argument_t_uint32:
		case _clltk_argument_t_int32: {
			const uint32_t value = va_arg(args_copy, __typeof__(uint32_t));
			*(uint32_t *)buffer = value;
			buffer += fix_arg_size;
		} break;
		case _clltk_argument_t_uint64:
		case _clltk_argument_t_int64:
		case _clltk_argument_t_pointer: {
			const uint64_t value = va_arg(args_copy, __typeof__(uint64_t));
			*(uint64_t *)buffer = value;
			buffer += fix_arg_size;
		} break;
		case _clltk_argument_t_uint128:
		case _clltk_argument_t_int128: {
			const __uint128_t value = va_arg(args_copy, __typeof__(__uint128_t));
			*(__uint128_t *)buffer = value;
			buffer += fix_arg_size;
		} break;
#if !defined(__KERNEL__) // todo create an error?
		case _clltk_argument_t_float: {
			const float value = (float)va_arg(args_copy, __typeof__(double));
			*(float *)buffer = value;
			buffer += fix_arg_size;
		} break;
		case _clltk_argument_t_double: {
			const double value = va_arg(args_copy, __typeof__(double));
			*(double *)buffer = value;
			buffer += fix_arg_size;
		} break;
#endif
		case _clltk_argument_t_string: {
			const char *const value = va_arg(args_copy, __typeof__(value));

			const uint32_t string_size = (uint32_t)(sizes[arg_index] - fix_arg_size);
			memcpy((void *)buffer, &string_size, fix_arg_size);
			buffer += fix_arg_size;

			memcpy((void *)buffer, value, string_size - 1);
			memset((void *)(buffer + string_size - 1), 0, 1); // add null terminator to string
			buffer += string_size;
		} break;
		case _clltk_argument_t_unknown:
		default:
			break;
		}
	}
	va_end(args_copy);
}
