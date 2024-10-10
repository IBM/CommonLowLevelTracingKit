// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ringbuffer/ringbuffer.h"
#include "gtest/gtest.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdlib.h>
#include <string>
#include <vector>

class ringbuffer : public ::testing::Test
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
class ringbuffer_in : public ringbuffer
{
};
class ringbuffer_out : public ringbuffer
{
};

TEST_F(ringbuffer_out, out_from_brand_new)
{
	char data[10] = {0};
	EXPECT_EQ(0, ::ringbuffer_out(data, sizeof(data), rb));
}

TEST_F(ringbuffer_out, out_from_null)
{
	char data[10] = {0};
	EXPECT_EQ(0, ::ringbuffer_out(data, sizeof(data), nullptr));
}

TEST_F(ringbuffer_out, out_size_null)
{
	char in[] = "ABC";
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	char data[10] = {0};
	EXPECT_EQ(0, ::ringbuffer_out(data, 0, rb));
}
TEST_F(ringbuffer_out, out_to_null)
{
	char in[] = "ABC";
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	char data[10] = {0};
	EXPECT_EQ(0, ::ringbuffer_out(nullptr, sizeof(data), rb));
}

TEST_F(ringbuffer_in, in_to_brand_new)
{
	char data[10] = {0};
	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
}

TEST_F(ringbuffer_in, in_to_big)
{
	char data[space_size - sizeof(ringbuffer_entry_head_t)] = {0};
	EXPECT_EQ(0, ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(0, rb->wrapped);
	EXPECT_EQ(0, rb->dropped);
}

TEST_F(ringbuffer_in, max_size)
{
	char data[space_size - sizeof(ringbuffer_entry_head_t) - 2] = {0};
	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(0, ringbuffer_available(rb));
	EXPECT_EQ(sizeof(data) + sizeof(ringbuffer_entry_head_t) + 1, ringbuffer_occupied(rb));
	EXPECT_EQ(0, rb->dropped);
	EXPECT_EQ(0, rb->wrapped);
}

TEST_F(ringbuffer_in, in_with_wrapped)
{
	char data[space_size * 2 / 3] = {0};
	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(0, rb->wrapped);
	EXPECT_EQ(0, rb->dropped);
	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(1, rb->wrapped);
	EXPECT_EQ(1, rb->dropped);
}

TEST_F(ringbuffer_in, from_null)
{
	EXPECT_EQ(0, ::ringbuffer_in(rb, nullptr, 10));
	EXPECT_TRUE(::ringbuffer_valid(rb));
	EXPECT_EQ(0, rb->wrapped);
	EXPECT_EQ(0, rb->dropped);
	EXPECT_EQ(0, ringbuffer_occupied(rb));
}

TEST_F(ringbuffer_in, zero_size)
{
	char data[10] = {0};
	EXPECT_EQ(0, ::ringbuffer_in(rb, data, 0));
	EXPECT_TRUE(::ringbuffer_valid(rb));
	EXPECT_EQ(0, rb->wrapped);
	EXPECT_EQ(0, rb->dropped);
	EXPECT_EQ(0, ringbuffer_occupied(rb));
}

TEST_F(ringbuffer, in_than_out)
{
	char in[] = "ABC";
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));
	char out[sizeof(in)] = {0};
	EXPECT_LE(sizeof(out), ::ringbuffer_out(out, sizeof(out), rb));
	EXPECT_EQ(std::string(in), std::string(out));
}

TEST_F(ringbuffer, entry_sizes)
{
	constexpr uint8_t max_entry_size = 10;
	for (uint8_t entry_size = 1; entry_size <= max_entry_size; entry_size++) {
		uint8_t in[max_entry_size] = {0};
		in[entry_size - 1] = 'A' + entry_size;
		EXPECT_EQ(entry_size, ::ringbuffer_in(rb, in, entry_size));
	}

	ASSERT_EQ(0, rb->wrapped);
	for (uint8_t entry_size = 1; entry_size <= max_entry_size; entry_size++) {
		uint8_t out[max_entry_size] = {0};
		EXPECT_EQ(entry_size, ::ringbuffer_out(out, entry_size, rb));
		EXPECT_EQ('A' + entry_size, out[entry_size - 1]);
	}
	EXPECT_EQ(0, ringbuffer_occupied(rb));
}

TEST_F(ringbuffer, in_twice_than_out_once)
{
	// first in
	char in[] = "ABC";
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));

	// second in
	char in_second[] = "Z";
	EXPECT_EQ(sizeof(in_second), ::ringbuffer_in(rb, in_second, sizeof(in_second)));

	// out
	char out[sizeof(in)] = {0};
	EXPECT_LE(sizeof(out), ::ringbuffer_out(out, sizeof(out), rb));
	EXPECT_EQ(std::string(in), std::string(out));
}

TEST_F(ringbuffer, in_once_than_out_twice)
{
	// in
	char in[] = "ABC";
	EXPECT_EQ(sizeof(in), ::ringbuffer_in(rb, in, sizeof(in)));

	// first out
	char out[sizeof(in)] = {0};
	EXPECT_LE(sizeof(out), ::ringbuffer_out(out, sizeof(out), rb));
	EXPECT_EQ(std::string(in), std::string(out));

	// second out
	EXPECT_EQ(0, ::ringbuffer_out(nullptr, 0, rb));
}

class ringbuffer_occupied : public ringbuffer
{
};
TEST_F(ringbuffer_occupied, brand_new)
{
	EXPECT_EQ(0, ::ringbuffer_occupied(rb));
}

TEST_F(ringbuffer_occupied, once_in)
{
	char data[10] = {0};
	ringbuffer_in(rb, data, sizeof(data));
	EXPECT_LE(sizeof(data), ::ringbuffer_occupied(rb));
}

TEST_F(ringbuffer_occupied, twice_in)
{
	char data[10] = {0};
	ringbuffer_in(rb, data, sizeof(data));
	const size_t overhead = ::ringbuffer_occupied(rb) - sizeof(data);
	EXPECT_EQ(1 * (sizeof(data) + overhead), ::ringbuffer_occupied(rb));
	ringbuffer_in(rb, data, sizeof(data));
	EXPECT_EQ(2 * (sizeof(data) + overhead), ::ringbuffer_occupied(rb));
}

TEST_F(ringbuffer_occupied, after_wrapped_dropped_everything)
{
	char data[space_size * 2 / 3] = {0};

	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(0, rb->wrapped);
	const size_t overhead = ::ringbuffer_occupied(rb) - sizeof(data);
	EXPECT_EQ(1 * (sizeof(data) + overhead), ::ringbuffer_occupied(rb));

	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(1, rb->wrapped);
	EXPECT_EQ(1 * (sizeof(data) + overhead), ::ringbuffer_occupied(rb));
}

TEST_F(ringbuffer_occupied, after_wrapped_dropped_partial)
{
	char data[space_size * 2 / 5] = {0};

	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	const size_t overhead = ::ringbuffer_occupied(rb) - sizeof(data);
	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(0, rb->wrapped);
	EXPECT_EQ(2 * (sizeof(data) + overhead), ::ringbuffer_occupied(rb));

	EXPECT_EQ(sizeof(data), ::ringbuffer_in(rb, data, sizeof(data)));
	EXPECT_EQ(1, rb->wrapped);
	EXPECT_EQ(2 * (sizeof(data) + overhead), ::ringbuffer_occupied(rb));
}

TEST(ringbuffer_test, loop_test)
{
	constexpr size_t space_size = 1024 + sizeof(ringbuffer_head_t);
	void *const space = malloc(space_size);
	ringbuffer_head_t *rb = ringbuffer_init(space, space_size);
	EXPECT_NE(rb, nullptr);
	std::vector<std::string> data = {
		"",
		"0",
		"01",
		"012",
		"0123",
		"01234",
		"012345",
		"0123456",
		"01234567",
		"012345678",
		"0123456789",
		"0123456789a",
		"0123456789ab",
		"0123456789abc",
		"0123456789abcd",
		"0123456789abcde",
		"0123456789abcdef",
		"0123456789abcdefg",
		"0123456789abcdefgh",
		"0123456789abcdefghi",
		"0123456789abcdefghij",
		"0123456789abcdefghijk",
		"0123456789abcdefghijkl",
		"0123456789abcdefghijklm",
		"0123456789abcdefghijklmn",
		"0123456789abcdefghijklmno",
		"0123456789abcdefghijklmnop",
		"0123456789abcdefghijklmnopq",
		"0123456789abcdefghijklmnopqr",
		"0123456789abcdefghijklmnopqrs",
		"0123456789abcdefghijklmnopqrst",
		"0123456789abcdefghijklmnopqrstu",
		"0123456789abcdefghijklmnopqrstuv",
		"0123456789abcdefghijklmnopqrstuvw",
		"0123456789abcdefghijklmnopqrstuvwx",
		"0123456789abcdefghijklmnopqrstuvwxy",
		"0123456789abcdefghijklmnopqrstuvwxyz",
		"0123456789abcdefghijklmnopqrstuvwxyzA",
		"0123456789abcdefghijklmnopqrstuvwxyzAB",
		"0123456789abcdefghijklmnopqrstuvwxyzABC",
		"0123456789abcdefghijklmnopqrstuvwxyzABCD",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDE",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEF",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFG",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGH",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHI",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJ",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJK",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKL",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNO",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQ",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQR",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRST",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVW",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXY",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'(",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-.",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=>",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=>?",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=>?@",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=>?@[",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=>?@[\\",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./:;<=>?@[\\]",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^_",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^_`",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^_`{",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^_`{|",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^_`{|}",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^_`{|}~",
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&'()*+,-./"
		":;<=>?@[\\]^_`{|}~ "};

	for (size_t i = 0; i < 100; i++) {

		for (size_t index = 0; index < data.size(); index++) {
			const auto &entry = data.at(index);
			ASSERT_EQ(ringbuffer_occupied(rb), 0) << "i = " << i << " index = " << index;
			EXPECT_EQ(ringbuffer_in(rb, entry.data(), entry.size()), entry.size())
				<< "i = " << i << " index = " << index;
			std::array<char, 1024> out;
			ASSERT_EQ(ringbuffer_out(out.data(), out.size(), rb), entry.size())
				<< "i = " << i << " index = " << index;
			ASSERT_EQ(ringbuffer_occupied(rb), 0) << "i = " << i << " index = " << index;
		}
	}
	free(space);
}

TEST(ringbuffer_test, next_free_vs_last_valid_overlap)
{
	constexpr size_t ringbuffer_body_capacity = 34;
	constexpr size_t ringbuffer_space_size = sizeof(ringbuffer_head_t) + ringbuffer_body_capacity +
											 1; // +1 due to next_free and last_valid overlap
	std::array<uint8_t, ringbuffer_space_size> space{0};

	constexpr size_t entry_overhead = 5;

	const std::array<uint8_t, 2> buffer{'A', 0};
	constexpr size_t entry_size = buffer.size() + entry_overhead;

	// first add one entry to check overhead
	for (std::vector<uint8_t>::size_type first_entry_data_size = 1; first_entry_data_size < 8;
		 first_entry_data_size++) {
		ringbuffer_head_t *rb = ringbuffer_init(space.data(), ringbuffer_space_size);
		EXPECT_NE(rb, nullptr);
		ASSERT_EQ(ringbuffer_available(rb), ringbuffer_body_capacity);
		ASSERT_EQ(ringbuffer_occupied(rb), 0);

		{
			const std::vector<uint8_t> first_buffer(first_entry_data_size, '1');
			const size_t first_entry_size = first_buffer.size() + entry_overhead;
			const size_t in = ringbuffer_in(rb, first_buffer.data(), first_buffer.size());
			ASSERT_EQ(in, first_buffer.size());
			ASSERT_EQ(ringbuffer_available(rb), ringbuffer_body_capacity - first_entry_size);
			ASSERT_EQ(ringbuffer_occupied(rb), first_entry_size);
		}

		// fill ringbuffer until next entry might not fit in
		do {
			const size_t in = ringbuffer_in(rb, buffer.data(), buffer.size());
			ASSERT_EQ(in, buffer.size());
		} while (ringbuffer_available(rb) > entry_size);

		// add next entry
		{
			ASSERT_LE(ringbuffer_available(rb), entry_size);
			ASSERT_GE(ringbuffer_occupied(rb), ringbuffer_body_capacity - entry_size);
			const size_t in = ringbuffer_in(rb, buffer.data(), buffer.size());
			ASSERT_EQ(in, buffer.size());
			ASSERT_LT(ringbuffer_available(rb), entry_size);
			ASSERT_GT(ringbuffer_occupied(rb), ringbuffer_body_capacity - entry_size);
		}
	}
}
