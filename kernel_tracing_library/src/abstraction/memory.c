// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/memory.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

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
