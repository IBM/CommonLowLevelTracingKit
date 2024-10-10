// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ringbuffer/ringbuffer.h"
#include "gtest/gtest.h"
#include <array>
#include <stdint.h>
#include <stdlib.h>
#include <string>

class recovery : public ::testing::Test
{
  public:
	ringbuffer_head_t *rb;
	constexpr static inline size_t space_size = 4096;

  protected:
	void SetUp() override
	{
		rb = ringbuffer_init(malloc(space_size + sizeof(ringbuffer_head_t)),
							 space_size + sizeof(ringbuffer_head_t));
	}
	void TearDown() override { free(rb); }
	void reset() { rb = ringbuffer_init(rb, space_size + sizeof(ringbuffer_head_t)); }
};

TEST_F(recovery, damage_oldest_entry_before_out)
{
	for (size_t i = 0; i < (sizeof(ringbuffer_entry_head_t) + 9 + 1); i++) {
		reset(); // reset ringbuffer
		EXPECT_EQ(0, ringbuffer_occupied(rb));
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 9> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 10> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 11> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};

		rb->body[i] += 1; // damage entry
		std::array<uint8_t, 32> data = {};
		// we will not get entry with size 9
		EXPECT_EQ(10, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(11, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(0, ::ringbuffer_out(data.data(), data.size(), rb));
	}
}
TEST_F(recovery, damage_second_oldest_entry_before_out)
{
	const size_t from = (sizeof(ringbuffer_entry_head_t) + 9 + 1);
	const size_t till = from + (sizeof(ringbuffer_entry_head_t) + 10 + 1);
	for (size_t i = from; i < till; i++) {
		reset(); // reset ringbuffer
		EXPECT_EQ(0, ringbuffer_occupied(rb));
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 9> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 10> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 11> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		rb->body[i] += 1; // damage entry
		std::array<uint8_t, 32> data = {};
		// we will not get entry with size 10
		EXPECT_EQ(9, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(11, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(0, ::ringbuffer_out(data.data(), data.size(), rb));
	}
}
TEST_F(recovery, damage_newest_entry_before_out)
{
	const size_t till = (sizeof(ringbuffer_entry_head_t) + 11 + 1);
	for (size_t i = 1; i <= till; i++) {
		reset(); // reset ringbuffer
		EXPECT_EQ(0, ringbuffer_occupied(rb));
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 9> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 10> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 11> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		rb->body[rb->next_free - i] += 1; // damage entry
		std::array<uint8_t, 32> data = {};
		// we will not get entry with size 11
		EXPECT_EQ(9, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(10, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(0, ::ringbuffer_out(data.data(), data.size(), rb));
	}
}

TEST_F(recovery, damage_oldest_entry_before_adding_next)
{
	for (size_t i = 0; i < (sizeof(ringbuffer_entry_head_t) + 9 + 1); i++) {
		reset(); // reset ringbuffer
		EXPECT_EQ(0, ringbuffer_occupied(rb));
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 9> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		rb->body[i] += 1; // damage entry
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 10> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 11> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};

		std::array<uint8_t, 32> data = {};
		// we will not get entry with size 9
		EXPECT_EQ(10, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(11, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(0, ::ringbuffer_out(data.data(), data.size(), rb));
	}
}
TEST_F(recovery, damage_second_oldest_entry_before_adding_next)
{
	const size_t from = (sizeof(ringbuffer_entry_head_t) + 9 + 1);
	const size_t till = from + (sizeof(ringbuffer_entry_head_t) + 10 + 1);
	for (size_t i = from; i < till; i++) {
		reset(); // reset ringbuffer
		EXPECT_EQ(0, ringbuffer_occupied(rb));
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 9> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 10> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		rb->body[i] += 1; // damage entry
		{
			const uint64_t occupied = ringbuffer_occupied(rb);
			std::array<uint8_t, 11> data = {};
			EXPECT_EQ(data.size(), ::ringbuffer_in(rb, data.data(), data.size()));
			EXPECT_EQ((sizeof(ringbuffer_entry_head_t) + data.size() + 1),
					  (ringbuffer_occupied(rb) - occupied));
		};
		std::array<uint8_t, 32> data = {};
		// we will not get entry with size 10
		EXPECT_EQ(9, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(11, ::ringbuffer_out(data.data(), data.size(), rb));
		EXPECT_EQ(0, ::ringbuffer_out(data.data(), data.size(), rb));
	}
}

class recovery_drop_test : public recovery
{
  protected:
	static constexpr uint64_t body_crc_size = 1;
	static constexpr uint64_t entry0_body_size = 10;
	static constexpr uint64_t entry0_size =
		sizeof(ringbuffer_entry_head_t) + entry0_body_size + body_crc_size;
	static constexpr uint64_t entry1_body_size = 20;
	static constexpr uint64_t entry1_size =
		sizeof(ringbuffer_entry_head_t) + entry1_body_size + body_crc_size;
	static constexpr uint64_t entry2_size =
		space_size - entry0_size - entry1_size - body_crc_size - 2; // -2 just to get some space
	static constexpr uint64_t entry2_body_size = entry2_size - sizeof(ringbuffer_entry_head_t) - 1;
	static constexpr uint64_t entry3_body_size = 1;
	static constexpr uint64_t entry3_size =
		sizeof(ringbuffer_entry_head_t) + entry3_body_size + body_crc_size;

	void SetUp() override
	{
		recovery::SetUp();
		rb->last_valid = rb->next_free = 2;
		{
			std::array<uint8_t, entry0_body_size> data = {};
			ringbuffer_in(rb, data.data(), data.size());
			EXPECT_EQ(ringbuffer_occupied(rb), entry0_size);
		};
		{
			std::array<uint8_t, entry1_body_size> data = {};
			ringbuffer_in(rb, data.data(), data.size());
			EXPECT_EQ(ringbuffer_occupied(rb), entry0_size + entry1_size);
		};
		{
			std::array<uint8_t, entry2_body_size> data = {};
			EXPECT_GE(ringbuffer_available(rb), entry2_size);
			ringbuffer_in(rb, data.data(), data.size());
			EXPECT_GE(ringbuffer_available(rb), 0);
		}
	}
};

TEST_F(recovery_drop_test, drop_damaged)
{
	rb->body[rb->last_valid] += 1; // destroy enty0
	{
		std::array<uint8_t, entry3_body_size> data = {};
		ringbuffer_in(rb, data.data(), data.size());
	}

	{
		std::array<uint8_t, space_size> data = {};
		EXPECT_EQ(entry1_body_size, ringbuffer_out(data.data(), data.size(), rb));
	}
}

TEST_F(recovery_drop_test, changed_last_valid)
{
	rb->last_valid += 1; // destroy entry 0
	{
		std::array<uint8_t, entry3_body_size> data = {};
		ringbuffer_in(rb, data.data(), data.size());
	}
	{
		std::array<uint8_t, space_size> data = {};
		EXPECT_EQ(entry1_body_size, ringbuffer_out(data.data(), data.size(), rb));
	}
}