// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Only true if both arguments which must be types are equal.
#define is_type(_T_, _V_) _Generic((_V_), _T_: true, default: false)

#if defined(__KERNEL__)
#include <asm/cacheflush.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#define alloca(size) __builtin_alloca(size)

static inline int vasprintf(char **__restrict __ptr, const char *__restrict __f, va_list __arg)
{
	*__ptr = kvasprintf(GFP_KERNEL, __f, __arg);
	return 0;
}

#else
#include <alloca.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#endif

void *memcpy_and_flush(void *dest, const void *src, size_t count);

void *memory_heap_allocation(size_t size) __attribute__((malloc, returns_nonnull));
void *memory_heap_realloc(void *old_ptr, size_t new_size) __attribute__((returns_nonnull));
void memory_heap_free(const void *ptr) __attribute__((nonnull));
size_t memory_get_page_size(void);

/* allocates space on stack.
 *  ptr must be pointer to pointer, like void**
 */
#define stack_alloc(ptr, size)                                                            \
	do {                                                                                  \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wvla\"") *ptr = \
			alloca(size);                                                                 \
		_Pragma("GCC diagnostic pop")                                                     \
	} while (0)

#ifdef __cplusplus
}
#endif
