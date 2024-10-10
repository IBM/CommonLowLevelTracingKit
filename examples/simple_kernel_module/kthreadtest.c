// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "kthreadtest.h"
#include "CommonLowLevelTracingKit/tracing.h"
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/types.h>

CLLTK_TRACEBUFFER(kthreadtest, 1024 * 1024);

struct thread {
	struct task_struct *handler;
	atomic64_t tp_count;
	struct completion *start_signal;
	uint64_t tid;
};
static int thread_function(void *data)
{
	if (data == NULL)
		return 1;
	struct thread *const t = data;
	wait_for_completion(t->start_signal);
	while (!kthread_should_stop()) {
		CLLTK_TRACEPOINT(kthreadtest, "tracepoint %llu", t->tid);
		atomic64_inc(&t->tp_count);
	}
	return 0;
}

#define THREAD_COUNT_MAX 32
static void run_threads(void)
{
	struct completion start_signal;
	init_completion(&start_signal);

	struct thread threads[THREAD_COUNT_MAX] = {0};

	const size_t half_cpu = max_t(size_t, num_online_cpus() >> 1, 1);
	const size_t thread_count = min_t(size_t, half_cpu, THREAD_COUNT_MAX);
	for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
		struct thread *const t = &threads[thread_index];
		atomic64_set(&t->tp_count, 0);
		t->start_signal = &start_signal;
		t->tid = thread_index;
		t->handler = kthread_run(thread_function, t, "my_kthread");
		if (IS_ERR(t->handler)) {
			pr_err("Failed to create the kthread\n");
		}
	}
	complete_all(&start_signal);
	for (size_t i = 0; i < 2; i++) {
		ssleep(1);
		printk("sleeped for %lus\n", (i + 1));
	}

	uint64_t total_tp_count = 0;
	for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
		struct thread *const t = &threads[thread_index];
		if (!IS_ERR(t->handler))
			kthread_stop(t->handler);
		printk("[%3lu] tp count = %6llu", thread_index, atomic64_read(&t->tp_count));
		total_tp_count += atomic64_read(&t->tp_count);
	}
	printk("toal  tp count = %6llu\n", total_tp_count);
}

void kthreadtest(void)
{
	printk("start kthreadtest\n");
	run_threads();
	printk("end kthreadtest\n");
}
