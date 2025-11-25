// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/sync.h"
#include "abstraction/error.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

static struct {
	pthread_mutex_t global_lock;
} context = {.global_lock = PTHREAD_MUTEX_INITIALIZER};

_Static_assert(sizeof(sync_mutex_t) >= sizeof(pthread_mutex_t),
			   "sizeof(sync_mutex_t) >= sizeof(pthread_mutex_t) must be true");

syn_lock_object_t sync_global_lock_get()
{
	const bool failed = (0 != pthread_mutex_lock(&context.global_lock));
	// pthread_mutex_lock will not return at raised signal so any error return is a real error
	if (failed) {
		ERROR_AND_EXIT("sync_global_lock_get failed with %s", strerror(errno));
	}
	return (syn_lock_object_t){&context.global_lock, true, NULL};
}

void syn_global_lock_release(syn_lock_object_t *const lock)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)lock->internal;
	if (lock->locked != true) {
		ERROR_AND_EXIT("syn_global_lock_release for unlocked mutex");
	}
	const bool failed = (0 != pthread_mutex_unlock(mutex));
	if (failed) {
		ERROR_AND_EXIT("syn_global_lock_release failed with %s", strerror(errno));
	}
	lock->locked = false;
}

void sync_memory_mutex_init(sync_mutex_t *const ptr)
{
	pthread_mutexattr_t attr = {0};
	int rc = 0;
	rc += pthread_mutexattr_init(&attr);
	rc += pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_NONE);
	rc += pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	rc += pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	rc += pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
	if (rc != 0) {
		ERROR_AND_EXIT("sync_memory_mutex_init failed while initializing mutex attributes");
	}

	pthread_mutex_t *mutex = (pthread_mutex_t *)ptr;
	const int status = pthread_mutex_init(mutex, &attr);
	if (status != 0) {
		ERROR_AND_EXIT("sync_memory_mutex_init failed with %s", strerror(status));
	}

	rc = 0;
	rc += pthread_mutexattr_destroy(&attr);
	if (rc != 0) {
		ERROR_LOG("pthread_mutexattr_destroy failed while destroying mutex attributes");
	}
}

syn_lock_object_t sync_memory_mutex_get(sync_mutex_t *const ptr)
{
	// us CLOCK_MONOTONIC because we don't want to timeout
	// if system lock gets adjusted from 1970 to current time
	// and CLOCK_MONOTONIC will not be updated in this case

	struct timespec abs_timeout = {0};
	if (clock_gettime(CLOCK_MONOTONIC, &abs_timeout) == -1)
		ERROR_AND_EXIT("reading system time failed");
	abs_timeout.tv_sec += 2; // 1 second timeout from current time on

	pthread_mutex_t *mutex = (pthread_mutex_t *)ptr;
	const int status = pthread_mutex_clocklock(mutex, CLOCK_MONOTONIC, &abs_timeout);
	if (status == 0) {
		// if no error occurred than return valid lock object
		return CREATE_SYN_LOCK_OBJECT(ptr, true, "");
	} else if (status == EOWNERDEAD) {
		// dead owner therefore we recover the state
		// we also got the lock
		if (pthread_mutex_consistent(mutex) == 0) {
			return CREATE_SYN_LOCK_OBJECT(ptr, true, "mutex recovered from dead owner");
		} else {
			return CREATE_SYN_LOCK_OBJECT(ptr, false, strerror(status));
		}
	} else {
		// if any other error occurred than return object with error info
		return CREATE_SYN_LOCK_OBJECT(ptr, false, strerror(status));
	}
}

void sync_memory_mutex_release(syn_lock_object_t *const lock)
{
	if (!lock->locked) {
		if (*lock->error_msg == '\0') {
			ERROR_LOG("releasing an unlocked mutex is not allowed");
			return;
		} else {
			// an error while locking occurred therefore do nothing in clean up function
			return;
		}
	}
	if (!lock->internal) {
		ERROR_AND_EXIT("releasing a NULL lock is not allowed");
	}
	pthread_mutex_t *mutex = (pthread_mutex_t *)lock->internal;
	const bool failed = (0 != pthread_mutex_unlock(mutex));
	if (failed) {
		ERROR_AND_EXIT("sync_memory_mutex_release failed");
	}
	lock->locked = false;
	lock->error_msg = "";
}
