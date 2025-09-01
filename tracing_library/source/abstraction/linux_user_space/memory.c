// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/memory.h"
#include "abstraction/error.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t page_size = 0;

void init_page_size(void)
{
	page_size = (size_t)getpagesize();
}

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
	void *ptr = malloc(size);
	if (!ptr) {
		ERROR_AND_EXIT("memory_heap_allocation failed");
	}
	memset(ptr, 0, size);
	return ptr;
}

void *memory_heap_realloc(void *old_ptr, size_t new_size)
{
	return realloc(old_ptr, new_size);
}

void memory_heap_free(const void *ptr)
{
	free((void *)ptr);
}

size_t memory_get_page_size(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, init_page_size);
	return page_size;
}
