// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include <array>
#include <stdint.h>
#include <string>
#include <sys/wait.h>
#include <syscall.h>
#include <thread>
#include <unistd.h>

pid_t gettid(void)
{
	return (pid_t)syscall(SYS_gettid);
}

CLLTK_TRACEBUFFER(ProcessAndThread, 256 * 1024);

static int func(char const *msg)
{
	thread_local std::string local_msg = {msg};
	auto pid = (uint64_t)getpid();
	auto tid = (uint64_t)gettid();
	for (size_t i = 0; i < 100; i++) {
		CLLTK_TRACEPOINT(ProcessAndThread, "pid=%lu tid=%lu location=%s i=%02lu", pid, tid,
						 local_msg.c_str(), i);
	}

	return 0;
}

constexpr size_t t_n = 10;
void run_threads(std::string str)
{
	std::array<std::thread, t_n> ths;
	std::array<std::string, t_n> strings;
	for (size_t thread_counter = 0; thread_counter < t_n; thread_counter++) {
		strings.at(thread_counter) = str + "_" + std::to_string(thread_counter);
		ths.at(thread_counter) = std::thread(&func, strings.at(thread_counter).data());
	}
	func(str.data());
	for (auto &th : ths)
		th.join();
}

int main(void)
{
	int status = 0;
	if (fork()) { // parent
		run_threads("parent");
		// wait for child
		while (wait(&status) > 0)
			;
	} else { // child
		run_threads("child");
	}

	return 0;
}
