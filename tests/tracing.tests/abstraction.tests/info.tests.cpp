// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/info.h"
#include "unistd.h"
#include "gtest/gtest.h"
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
TEST(info, get_timestamp)
{
	const auto t0 = info_get_timestamp_ns();
	const auto t1 = info_get_timestamp_ns();
	const auto t2 = info_get_timestamp_ns();
	EXPECT_TRUE(t0);
	EXPECT_LT(t0, t1);
	EXPECT_LT(t1, t2);
}

TEST(info, get_process_id_one_process)
{
	int pid_A = getpid();
	uint32_t pid_B = info_get_process_id();
	EXPECT_TRUE(pid_A);
	EXPECT_EQ(pid_A, pid_B);
}

TEST(info, get_process_id_two_process)
{
	int pid_A = getpid();
	ASSERT_TRUE(pid_A);
	static uint32_t *const pid_B = (uint32_t *)mmap(nullptr, sizeof(*pid_B), PROT_READ | PROT_WRITE,
													MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	ASSERT_TRUE(pid_B);
	*pid_B = 0;
	if (fork() == 0) {
		*pid_B = info_get_process_id();
		exit(EXIT_SUCCESS);
	}
	wait(nullptr);
	EXPECT_TRUE(pid_B);
	EXPECT_NE(pid_A, *pid_B);
	munmap(pid_B, sizeof(*pid_B));
}