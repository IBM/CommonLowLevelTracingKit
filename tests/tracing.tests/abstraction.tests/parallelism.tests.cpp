// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/file.h"
#include "gtest/gtest.h"
#include <array>
#include <atomic>
#include <barrier>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <stdint.h>
#include <string>
#include <thread>
#include <tuple>

class parallelism : public ::testing::Test
{
  protected:
	void SetUp() override { file_reset(); }
};

TEST_F(parallelism, multi_thread_mmap_write)
{
	const char file_name[] = "multi_thread_mmap_write";
	std::array<std::unique_ptr<std::thread>, 1024> threads;
	std::barrier thread_count{threads.size()};

	// create file
	file_t *temp_fh =
		file_create_temp(file_name, threads.size() * sizeof(int) + sizeof(std::atomic<int>));
	EXPECT_TRUE(temp_fh);

	file_t *final_fh = file_temp_to_final(&temp_fh);
	EXPECT_TRUE(final_fh);
	{
		file_t *fh = file_try_get(file_name);
		EXPECT_TRUE(fh);
		file_drop(&fh);
	}

	auto thread_function = [&thread_count, file_name](int index) -> void {
		file_t *fh = file_try_get(file_name);
		EXPECT_TRUE(fh);
		int *ptr = (int *)file_mmap_ptr(fh);
		EXPECT_NE(ptr, nullptr);
		thread_count.arrive_and_wait();
		ptr[index] = index;
		file_drop(&fh);
	};

	// start threads
	for (uint64_t index = 0; index < threads.size(); index++) {
		auto &thread = threads.at(index);
		thread = std::make_unique<std::thread>(thread_function, index);
	}

	// join threads
	for (auto &thread : threads) {
		thread->join();
	}

	// check result
	{
		file_t *fh = file_try_get(file_name);
		EXPECT_TRUE(fh);
		int *ptr = (int *)file_mmap_ptr(fh);
		for (uint64_t index = 0; index < threads.size(); index++) {
			EXPECT_EQ(ptr[index], index) << "file offset = 0x" << std::hex << (index * sizeof(int));
		}
		file_drop(&fh);
	}
	file_drop(&final_fh);
}

TEST_F(parallelism, multi_thread_atomic_increment)
{
	const char file_name[] = "multi_thread_atomic_increment";
	std::array<std::unique_ptr<std::thread>, 1024> threads;
	std::barrier thread_count{threads.size()};

	static_assert(std::atomic<int>::is_always_lock_free);
	// create file
	file_t *temp_fh = file_create_temp(file_name, sizeof(int) * sizeof(std::atomic<int>));
	EXPECT_TRUE(temp_fh);
	file_t *final_fh = file_temp_to_final(&temp_fh);
	EXPECT_TRUE(final_fh);

	// init file
	{
		file_t *fh = file_try_get(file_name);
		ASSERT_TRUE(fh);
		int *ptr = (int *)file_mmap_ptr(fh);
		EXPECT_NE(ptr, nullptr);
		(*ptr) = 0;
		new (ptr + 1) std::atomic<int>{0};
		file_drop(&fh);
	}

	auto thread_function = [&thread_count, file_name](int index) -> void {
		file_t *fh = file_try_get(file_name);
		EXPECT_TRUE(fh);
		int *ptr = (int *)file_mmap_ptr(fh);
		EXPECT_NE(ptr, nullptr);
		thread_count.arrive_and_wait();
		(*ptr) += index; // WARNING: will create race condition
		*(std::atomic<int> *)(ptr + 1) += index;
		file_drop(&fh);
	};

	// start threads
	for (uint64_t index = 0; index < threads.size(); index++) {
		auto &thread = threads.at(index);
		thread = std::make_unique<std::thread>(thread_function, index);
	}

	// join threads
	for (auto &thread : threads) {
		thread->join();
	}

	// check result
	{
		const int n = std::ssize(threads) - 1;
		const int should_be = n * (n + 1) / 2;
		file_t *fh = file_try_get(file_name);
		EXPECT_TRUE(fh);
		const int *const ptr = (int *)file_mmap_ptr(fh);
		const int unsafe_result = *ptr;
		std::ignore = unsafe_result;
		EXPECT_EQ(should_be, *(std::atomic<int> *)(ptr + 1));
		file_drop(&fh);
	}
	file_drop(&final_fh);
}
