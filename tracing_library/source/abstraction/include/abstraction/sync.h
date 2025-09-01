// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdbool.h>
#endif

typedef struct {
	void *internal;
	bool locked;
	const char *error_msg;
} syn_lock_object_t;

#define CREATE_SYN_LOCK_OBJECT(_INTERNAL_, _LOCKED_, _MSG_) \
	(syn_lock_object_t)                                     \
	{                                                       \
		_INTERNAL_, _LOCKED_, _MSG_                         \
	}

#define SYNC_CLEANUP(_X_) __attribute__((unused, cleanup(_X_)))

// mutex to inter thread communication
syn_lock_object_t sync_global_lock_get(void);
void syn_global_lock_release(syn_lock_object_t *const lock) __attribute__((nonnull));

#define SYNC_GLOBAL_LOCK(_NAME_) \
	syn_lock_object_t _NAME_ SYNC_CLEANUP(syn_global_lock_release) = sync_global_lock_get()

// mutex for inter process and thread synchronization via mmapped space
struct __attribute__((__packed__, aligned(8))) sync_mutex_t {
	unsigned char _internal[64];
};
typedef struct sync_mutex_t sync_mutex_t;
void sync_memory_mutex_init(sync_mutex_t *const);

syn_lock_object_t sync_memory_mutex_get(sync_mutex_t *const);
void sync_memory_mutex_release(syn_lock_object_t *const lock) __attribute__((nonnull));

#define SYNC_MEMORY_LOCK(_NAME_, _PTR_) \
	syn_lock_object_t _NAME_ SYNC_CLEANUP(sync_memory_mutex_release) = sync_memory_mutex_get(_PTR_)

#ifndef __cplusplus
_Static_assert(sizeof(sync_mutex_t) == 64, "sync_mutex_t must have size 64");
#else
static_assert(sizeof(sync_mutex_t) == 64, "sync_mutex_t must have size 64");
#endif

#ifdef __cplusplus
}
#endif
