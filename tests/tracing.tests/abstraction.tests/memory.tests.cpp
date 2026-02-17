// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/memory.h"
#include "unistd.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
using namespace testing;

TEST(memory, heap_allocation)
{
	const size_t first_size = 1024 * memory_get_page_size();
	std::string_view data = "Hello World";
	char *ptr = std::bit_cast<char *>(memory_heap_allocation(first_size));
	memcpy_and_flush(ptr, data.data(), data.size() + 1);
	EXPECT_EQ(ptr, data);
	memory_heap_free(ptr);
#ifdef CLLTK_ASAN_ENABLED
	EXPECT_EXIT({ *ptr = 'A'; }, ::testing::ExitedWithCode(1), ".*");
	EXPECT_EXIT({ [[maybe_unused]] char c = *ptr; }, ::testing::ExitedWithCode(1), ".*");
#else
	EXPECT_EXIT({ *ptr = 'A'; }, ::testing::KilledBySignal(SIGSEGV), ".*");
	EXPECT_EXIT({ [[maybe_unused]] char c = *ptr; }, ::testing::KilledBySignal(SIGSEGV), ".*");
#endif
}

TEST(memory, real_relocate)
{
	// alloacted memory must be big to really trigger a relocation
	const size_t first_size = 1024 * memory_get_page_size();
	const size_t second_size = 2 * 1024 * memory_get_page_size();
	std::string_view data = "Hello World";
	char *ptrA = std::bit_cast<char *>(memory_heap_allocation(first_size));
	memcpy_and_flush(ptrA, data.data(), data.size() + 1);
	char *ptrB = std::bit_cast<char *>(memory_heap_realloc(ptrA, second_size));
	ASSERT_NE(ptrA, ptrB);
	EXPECT_EQ(ptrB, data);
	memory_heap_free(ptrB);
}

TEST(memory, equal_relocate)
{
	const size_t first_size = 1024 * memory_get_page_size();
	std::string_view data = "Hello World";
	char *ptrA = std::bit_cast<char *>(memory_heap_allocation(first_size));
	memcpy_and_flush(ptrA, data.data(), data.size() + 1);
	char *ptrB = std::bit_cast<char *>(memory_heap_realloc(ptrA, first_size));
	// ASAN's realloc always returns a new pointer to detect use-after-realloc bugs
#ifndef CLLTK_ASAN_ENABLED
	ASSERT_EQ(ptrA, ptrB);
#endif
	EXPECT_EQ(ptrB, data);
	memory_heap_free(ptrB);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-protector"
static void stack_allocation_test()
{
	// stack grows downwards
	volatile const size_t size = 20;

	void *stackA = nullptr;
	uintptr_t startA;
	uintptr_t endA;

	void *stackB = nullptr;
	uintptr_t startB;
	uintptr_t endB;

	void *stackC = nullptr;
	uintptr_t startC;
	uintptr_t endC;

	const auto func = [&]() noexcept { // new stack scope
		stack_alloc(&stackB, size);
		startB = (*std::bit_cast<uintptr_t *>(&stackB) + 0);
		endB = (*std::bit_cast<uintptr_t *>(&stackB) + size);
	};

	stack_alloc(&stackA, size);
	ASSERT_TRUE(stackA);
	startA = (*std::bit_cast<uintptr_t *>(&stackA) + 0);
	endA = (*std::bit_cast<uintptr_t *>(&stackA) + size);

	func();

	stack_alloc(&stackC, size);
	ASSERT_TRUE(stackC);
	startC = (*std::bit_cast<uintptr_t *>(&stackC) + 0);
	endC = (*std::bit_cast<uintptr_t *>(&stackC) + size);

	// layout in memory:
	//  <-( - )-                                                                  -( + )->
	//                                                          |<startA            endA>|
	//                                                          AAAAAAAAAAAAAAAAAAAAAAAAA|
	//                                          <func prolog?>
	// in nested     |<startB            endB>|
	// function      BBBBBBBBBBBBBBBBBBBBBBBBB|
	//                               |<startC            endC>|
	//                               CCCCCCCCCCCCCCCCCCCCCCCCC|
	//

	std::initializer_list<uintptr_t> li{startA, endA, startB, endB, startC, endC};
	auto it = std::min_element(li.begin(), li.end());
	const uintptr_t min = *it;
	startA -= min;
	endA -= min;
	startB -= min;
	endB -= min;
	startC -= min;
	endC -= min;
	printf("Memory layout of stack allocation\n");
	printf("%*s%-3lu A", (int)startA, "|<", startA);
	printf("%*s%3lu>|\n", (int)(endA - startA - 10), "", endA);
	printf("%*s%-3lu B", (int)startB, "|<", startB);
	printf("%*s%3lu>|\n", (int)(endB - startB - 10), "", endB);
	printf("%*s%-3lu C", (int)startC, "|<", startC);
	printf("%*s%3lu>|\n", (int)(endC - startC - 10), "", endC);

	EXPECT_LT(endB, startA) << "B should end before A starts";
	EXPECT_LT(endC, startA) << "C should end before A starts";

	EXPECT_GT(endC, startB) << "C and B should overlap";
}
#pragma GCC diagnostic pop

TEST(memory, stack_allocation)
{
	stack_allocation_test();
}
