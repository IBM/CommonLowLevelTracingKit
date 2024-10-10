// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/info.h"

#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/timekeeping.h>

uint64_t info_get_timestamp_ns(void)
{
	return ktime_get_real_fast_ns();
}
uint32_t info_get_thread_id(void)
{
	return task_pid_nr(current);
}
uint32_t info_get_process_id(void)
{
	return current->pid;
}
