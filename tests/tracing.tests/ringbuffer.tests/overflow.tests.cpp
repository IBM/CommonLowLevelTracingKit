// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ringbuffer/ringbuffer.h"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>

class ringbuffer_overflow : public ::testing::Test
{
  public:
	ringbuffer_head_t *rb;
	constexpr static inline size_t space_size = 256;

  protected:
	void SetUp() override
	{
		rb = ringbuffer_init(malloc(space_size + sizeof(ringbuffer_head_t)),
							 space_size + sizeof(ringbuffer_head_t));
	}
	void TearDown() override { free(rb); }
};

TEST_F(ringbuffer_overflow, dropped)
{
	for (size_t i = 0; i < space_size; i++) {
		char in[] = "ABC";
		EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	}

	rb->dropped = UINT64_MAX - 1;
	ASSERT_LT(rb->dropped, UINT64_MAX);
	char in[] = "ABC";
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	ASSERT_EQ(rb->dropped, UINT64_MAX);
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	ASSERT_EQ(rb->dropped, UINT64_MAX);
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	ASSERT_EQ(rb->dropped, UINT64_MAX);
}

TEST_F(ringbuffer_overflow, wrapped)
{
	rb->wrapped = UINT64_MAX - 1;
	ASSERT_LT(rb->wrapped, UINT64_MAX);
	for (size_t i = 0; i < space_size; i++) {
		char in[] = "ABC";
		EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	}
	ASSERT_EQ(rb->wrapped, UINT64_MAX);
	for (size_t i = 0; i < space_size; i++) {
		char in[] = "ABC";
		EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	}
	ASSERT_EQ(rb->wrapped, UINT64_MAX);
}