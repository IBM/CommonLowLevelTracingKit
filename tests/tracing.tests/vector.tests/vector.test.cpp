// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "c-vector/vec.h"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <stdint.h>
#include <string>

TEST(vector, vector_create)
{
	int *const vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	EXPECT_EQ(0, vector_size(vec));
	vector_free(&vec);
}

TEST(vector, one_entry)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	EXPECT_EQ(1, vector_size(vec));
	vector_free(&vec);
}

TEST(vector, two_entry)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	vector_add(&vec, 2);
	EXPECT_EQ(2, vector_size(vec));
	vector_free(&vec);
}

TEST(vector, add_until_relocated)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	int *old_vec = vec;
	int counter = 0;
	do {
		vector_add(&vec, counter);
		counter++;
	} while (old_vec == vec);
	ASSERT_TRUE(vec);
	EXPECT_EQ(counter, vector_size(vec));
	vector_free(&vec);
}

TEST(vector, uint8)
{
	uint8_t *vec = (uint8_t *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	vector_add(&vec, 2);
	EXPECT_EQ(2, vector_size(vec));
	EXPECT_EQ(1, vec[0]);
	EXPECT_EQ(2, vec[1]);
	vector_free(&vec);
}
TEST(vector, uint16)
{
	uint16_t *vec = (uint16_t *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	vector_add(&vec, 2);
	EXPECT_EQ(2, vector_size(vec));
	EXPECT_EQ(1, vec[0]);
	EXPECT_EQ(2, vec[1]);
	vector_free(&vec);
}

TEST(vector, struct)
{
	typedef struct {
		int i;
	} my_type_t;
	my_type_t *vec = (my_type_t *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, {1});
	vector_add(&vec, {2});
	EXPECT_EQ(2, vector_size(vec));
	EXPECT_EQ(1, vec[0].i);
	EXPECT_EQ(2, vec[1].i);
	vector_free(&vec);
}

bool matcher(const int *const vector_entry, const int *const search_value)
{
	return (vector_entry) && (search_value) && (*vector_entry == *search_value);
}

TEST(vector, find_in_empty)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	ASSERT_EQ(0, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	EXPECT_FALSE(match.found);
	EXPECT_EQ(0, match.position);
	EXPECT_EQ(0, match.entry);
	vector_free(&vec);
}

TEST(vector, find_not_in_one)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 2);
	ASSERT_EQ(1, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	EXPECT_FALSE(match.found);
	EXPECT_EQ(0, match.position);
	EXPECT_EQ(0, match.entry);
	vector_free(&vec);
}

TEST(vector, find_in_one)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	ASSERT_EQ(1, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	EXPECT_TRUE(match.found);
	EXPECT_EQ(0, match.position);
	EXPECT_EQ(vec, match.entry);
	vector_free(&vec);
}

TEST(vector, find_not_in_three)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 2);
	vector_add(&vec, 4);
	vector_add(&vec, 3);
	ASSERT_EQ(3, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	EXPECT_FALSE(match.found);
	EXPECT_EQ(0, match.position);
	EXPECT_EQ(0, match.entry);
	vector_free(&vec);
}

TEST(vector, find_in_three_first)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	vector_add(&vec, 2);
	vector_add(&vec, 3);
	ASSERT_EQ(3, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	EXPECT_TRUE(match.found);
	EXPECT_EQ(0, match.position);
	EXPECT_EQ(&vec[0], match.entry);
	vector_free(&vec);
}

TEST(vector, find_in_three_middle)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 2);
	vector_add(&vec, 1);
	vector_add(&vec, 3);
	ASSERT_EQ(3, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	EXPECT_TRUE(match.found);
	EXPECT_EQ(1, match.position);
	EXPECT_EQ(&vec[1], match.entry);
	vector_free(&vec);
}

TEST(vector, find_in_three_first_last)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 2);
	vector_add(&vec, 3);
	vector_add(&vec, 1);
	ASSERT_EQ(3, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	EXPECT_TRUE(match.found);
	EXPECT_EQ(2, match.position);
	EXPECT_EQ(&vec[2], match.entry);
	vector_free(&vec);
}

TEST(vector, remove)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 2);
	vector_add(&vec, 1);
	vector_add(&vec, 3);
	ASSERT_EQ(3, vector_size(vec));
	int search = 1;
	auto match = vector_find(vec, matcher, &search);
	vector_remove(vec, match.position);
	match = vector_find(vec, matcher, &search);
	EXPECT_FALSE(match.found);
	vector_free(&vec);
}
TEST(vector, erase)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	vector_add(&vec, 2);
	vector_add(&vec, 3);
	vector_add(&vec, 4);
	ASSERT_EQ(4, vector_size(vec));
	{
		vector_erase(vec, 1, 2);
	}
	ASSERT_EQ(2, vector_size(vec));
	{
		int search = 1;
		auto match = vector_find(vec, matcher, &search);
		EXPECT_EQ(match.position, 0);
		EXPECT_TRUE(match.found);
	}
	{
		int search = 2;
		auto match = vector_find(vec, matcher, &search);
		EXPECT_FALSE(match.found);
	}
	{
		int search = 3;
		auto match = vector_find(vec, matcher, &search);
		EXPECT_FALSE(match.found);
	}
	{
		int search = 4;
		auto match = vector_find(vec, matcher, &search);
		EXPECT_EQ(match.position, 1);
		EXPECT_TRUE(match.found);
	}
	vector_free(&vec);
}

TEST(vector, free)
{
	int *vec = (int *)vector_create();
	ASSERT_TRUE(vec);
	vector_add(&vec, 1);
	ASSERT_EQ(1, vector_size(vec));
	vector_free(&vec);
	EXPECT_FALSE(vec);
}
