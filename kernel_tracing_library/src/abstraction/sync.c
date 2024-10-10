// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/sync.h"
#include <linux/spinlock.h>

struct save_lock {
	spinlock_t lock;
	unsigned long flags;
};
static_assert(sizeof(sync_mutex_t) >= sizeof(struct save_lock),
			  "struct save_lock must not exceed sync_mutex_t");

static struct {
	struct save_lock global_lock;
} context = {
	.global_lock =
		{
			.lock = __SPIN_LOCK_UNLOCKED(global_lock.lock),
		},
};

syn_lock_object_t sync_global_lock_get(void)
{
	spin_lock_irqsave(&context.global_lock.lock, context.global_lock.flags);
	return (syn_lock_object_t){&context.global_lock, true, ""};
}

void syn_global_lock_release(syn_lock_object_t *const ptr)
{
	struct save_lock *lock = (struct save_lock *)ptr->internal;
	spin_unlock_irqrestore(&lock->lock, lock->flags);
}

void sync_memory_mutex_init(sync_mutex_t *const ptr)
{
	struct save_lock *lock = (struct save_lock *)ptr;
	spin_lock_init(&lock->lock);
}

syn_lock_object_t sync_memory_mutex_get(sync_mutex_t *const ptr)
{
	struct save_lock *lock = (struct save_lock *)ptr;
	spin_lock_irqsave(&lock->lock, lock->flags);
	return (syn_lock_object_t){lock, true, ""};
}

void sync_memory_mutex_release(syn_lock_object_t *const ptr)
{
	struct save_lock *lock = (struct save_lock *)ptr->internal;
	spin_unlock_irqrestore(&lock->lock, lock->flags);
}
