// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ringbuffer/ringbuffer.h"
#include "gtest/gtest.h"
#include <string>

TEST(ringbuffer_init, normal_size)
{
	char data[1024] = {0};
	ringbuffer_head_t *rb = ::ringbuffer_init(data, sizeof(data));
	EXPECT_TRUE(ringbuffer_valid(rb));
}

TEST(ringbuffer_init, 2MB)
{
	char data[2ull * 1024 * 1024] = {0};
	ringbuffer_head_t *rb = ::ringbuffer_init(data, sizeof(data));
	EXPECT_TRUE(ringbuffer_valid(rb));
}

TEST(ringbuffer_init, null)
{
	ringbuffer_head_t *rb = ::ringbuffer_init(nullptr, 1024);
	EXPECT_FALSE(ringbuffer_valid(rb));
}

TEST(ringbuffer_init, zero_size)
{
	char data[1024] = {0};
	ringbuffer_head_t *rb = ::ringbuffer_init(data, 0);
	EXPECT_FALSE(ringbuffer_valid(rb));
}

TEST(ringbuffer_init, to_small)
{
	char data[sizeof(ringbuffer_head_t) + 1] = {0};
	ringbuffer_head_t *rb = ::ringbuffer_init(data, sizeof(data));
	EXPECT_FALSE(ringbuffer_valid(rb));
}

TEST(ringbuffer_init, just_big_enough)
{
	char data[sizeof(ringbuffer_head_t) + 2] = {0};
	ringbuffer_head_t *rb = ::ringbuffer_init(data, sizeof(data));
	EXPECT_TRUE(ringbuffer_valid(rb));
}

TEST(ringbuffer_open, initialized)
{
	char data[1024] = {0};
	::ringbuffer_init(data, sizeof(data));
	ringbuffer_head_t *rb = ::ringbuffer_open(data);
	EXPECT_TRUE(ringbuffer_valid(rb));
	EXPECT_EQ(sizeof(data) - sizeof(ringbuffer_head_t) - 1, ringbuffer_capacity(rb));
}

TEST(ringbuffer_open, uninitialized)
{
	char data[1024] = {0};
	ringbuffer_head_t *rb = ::ringbuffer_open(data);
	EXPECT_FALSE(ringbuffer_valid(rb));
}

TEST(ringbuffer_open, null)
{
	ringbuffer_head_t *rb = ::ringbuffer_open(nullptr);
	EXPECT_FALSE(ringbuffer_valid(rb));
}

TEST(ringbuffer_open, after_out)
{
	char in[] = "ABDC";
	char space[1024] = {0};
	{
		ringbuffer_head_t *rb = ::ringbuffer_init(space, sizeof(space));
		::ringbuffer_in(rb, in, sizeof(in));
	}
	{
		char out[sizeof(in)] = {0};
		ringbuffer_head_t *rb = ::ringbuffer_open(space);
		EXPECT_EQ(sizeof(out), ::ringbuffer_out(out, sizeof(out), rb));
		EXPECT_EQ(std::string(in), std::string(out));
	}
}