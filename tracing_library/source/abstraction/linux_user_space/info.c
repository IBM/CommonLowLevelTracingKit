// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/info.h"
#include "abstraction/optimization.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

__thread _Atomic uint32_t cached_pid;
__thread _Atomic uint32_t cached_tip;

void update_cache(void)
{
	{
		const uint32_t value = (uint32_t)syscall(SYS_getpid);
		atomic_store(&cached_pid, value);
	}
	{
		const uint32_t value = (uint32_t)syscall(SYS_gettid);
		atomic_store(&cached_tip, value);
	}
}

static void only_once_called(void)
{
	pthread_atfork(NULL, NULL, update_cache);
	update_cache();
}

static void init(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, only_once_called);
}

uint64_t info_get_timestamp_ns()
{
	struct timespec t = {0};
	timespec_get(&t, TIME_UTC);
	return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

uint32_t info_get_thread_id()
{
	init();
	uint32_t value = atomic_load(&cached_tip);
	if (likely(value))
		return value;
	update_cache();
	return info_get_thread_id();
}

uint32_t info_get_process_id()
{
	init();
	uint32_t value = atomic_load(&cached_pid);
	if (likely(value))
		return value;
	update_cache();
	return info_get_process_id();
}
