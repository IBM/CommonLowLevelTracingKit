// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ringbuffer/ringbuffer.h"
#include "gtest/gtest.h"
#include <cstddef>
#include <cstdint>
#include <stdlib.h>
#include <string>

class ringbuffer_clear_test : public ::testing::Test
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

TEST_F(ringbuffer_clear_test, clear_empty_buffer)
{
	EXPECT_EQ(0, ringbuffer_occupied(rb));
	ringbuffer_clear(rb);
	EXPECT_EQ(0, ringbuffer_occupied(rb));
	EXPECT_EQ(0, rb->dropped);
	EXPECT_EQ(0, rb->entries);
}

TEST_F(ringbuffer_clear_test, clear_with_one_entry)
{
	char data[] = "test data";
	EXPECT_EQ(sizeof(data), ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(1, rb->entries);
	EXPECT_GT(ringbuffer_occupied(rb), 0);

	ringbuffer_clear(rb);

	EXPECT_EQ(0, ringbuffer_occupied(rb));
	EXPECT_EQ(1, rb->entries); // entries counter preserved
	EXPECT_EQ(1, rb->dropped); // dropped = entries after clear
}

TEST_F(ringbuffer_clear_test, clear_with_multiple_entries)
{
	char data1[] = "first";
	char data2[] = "second";
	char data3[] = "third";

	EXPECT_EQ(sizeof(data1), ringbuffer_in(rb, data1, sizeof(data1)));
	EXPECT_EQ(sizeof(data2), ringbuffer_in(rb, data2, sizeof(data2)));
	EXPECT_EQ(sizeof(data3), ringbuffer_in(rb, data3, sizeof(data3)));
	EXPECT_EQ(3, rb->entries);
	EXPECT_GT(ringbuffer_occupied(rb), 0);

	ringbuffer_clear(rb);

	EXPECT_EQ(0, ringbuffer_occupied(rb));
	EXPECT_EQ(3, rb->entries); // entries counter preserved
	EXPECT_EQ(3, rb->dropped); // dropped = entries after clear
}

TEST_F(ringbuffer_clear_test, clear_preserves_next_free_position)
{
	char data[] = "test";
	EXPECT_EQ(sizeof(data), ringbuffer_in(rb, data, sizeof(data)));

	uint64_t next_free_before = rb->next_free;
	ringbuffer_clear(rb);

	EXPECT_EQ(next_free_before, rb->next_free); // next_free unchanged
	EXPECT_EQ(rb->next_free, rb->last_valid);	// last_valid moved to next_free
}

TEST_F(ringbuffer_clear_test, clear_preserves_wrapped_counter)
{
	// Fill buffer to cause wrapping
	char data[space_size * 2 / 3] = {0};
	EXPECT_EQ(sizeof(data), ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(0, rb->wrapped);

	EXPECT_EQ(sizeof(data), ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(1, rb->wrapped);

	uint64_t wrapped_before = rb->wrapped;
	ringbuffer_clear(rb);

	EXPECT_EQ(wrapped_before, rb->wrapped); // wrapped counter preserved
}

TEST_F(ringbuffer_clear_test, clear_allows_new_entries)
{
	char data[] = "initial data";
	EXPECT_EQ(sizeof(data), ringbuffer_in(rb, data, sizeof(data)));

	ringbuffer_clear(rb);
	EXPECT_EQ(0, ringbuffer_occupied(rb));

	// Should be able to add new entries after clear
	char new_data[] = "new data after clear";
	EXPECT_EQ(sizeof(new_data), ringbuffer_in(rb, new_data, sizeof(new_data)));
	EXPECT_GT(ringbuffer_occupied(rb), 0);
	EXPECT_EQ(2, rb->entries); // cumulative entry count

	// Should be able to read the new entry
	char out[sizeof(new_data)] = {0};
	EXPECT_EQ(sizeof(new_data), ringbuffer_out(out, sizeof(out), rb));
	EXPECT_EQ(std::string(new_data), std::string(out));
}

TEST_F(ringbuffer_clear_test, clear_null_ringbuffer)
{
	// Should not crash when called with nullptr
	ringbuffer_clear(nullptr);
}

TEST_F(ringbuffer_clear_test, out_returns_nothing_after_clear)
{
	char data[] = "test";
	EXPECT_EQ(sizeof(data), ringbuffer_in(rb, data, sizeof(data)));

	ringbuffer_clear(rb);

	char out[sizeof(data)] = {0};
	EXPECT_EQ(0, ringbuffer_out(out, sizeof(out), rb));
}
