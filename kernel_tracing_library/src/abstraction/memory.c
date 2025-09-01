// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/memory.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

void *memcpy_and_flush(void *dest, const void *src, size_t count)
{
	void *restult_dest;
	restult_dest = memcpy(dest, src, count);

#ifdef __aarch64__
	for (uintptr_t ptr = (uintptr_t)dest; ptr < ((uintptr_t)dest + count); ptr += 64) {
		__asm__ volatile("dc cvac, %0" : : "r"(ptr) : "memory");
	}
	__asm__ volatile("dsb ish" : : : "memory");
	__asm__ volatile("isb" : : : "memory");
#else
	(void)(uintptr_t)0; // prevent the removal of header for uintptr_t
#endif
	return restult_dest;
}

void *memory_heap_allocation(size_t size)
{
	return kzalloc(size, GFP_KERNEL);
}

void *memory_heap_realloc(void *old_ptr, size_t new_size)
{
	return krealloc(old_ptr, new_size, GFP_KERNEL);
}

void memory_heap_free(const void *ptr)
{
	kfree(ptr);
}

size_t memory_get_page_size(void)
{
	return PAGE_SIZE;
}
