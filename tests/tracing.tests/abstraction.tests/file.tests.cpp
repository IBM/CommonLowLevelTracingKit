// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/file.h"
#include "gtest/gtest.h"
#include <string>

class file : public ::testing::Test
{
  protected:
	void SetUp() override { file_reset(); }
};

TEST_F(file, normal_flow_with_create)
{
	const char name[] = "normal_flow_with_create";
	{
		auto fh = ::file_try_get(name);
		EXPECT_EQ(fh, nullptr);
	}
	{
		auto temp_file = file_create_temp(name, 1024);
		EXPECT_NE(temp_file, nullptr);
		auto fh = file_temp_to_final(&temp_file);
		EXPECT_EQ(temp_file, nullptr);
		EXPECT_NE(fh, nullptr);
	}
}

TEST_F(file, open_file_twice)
{
	const char name[] = "open_file_twice";
	{
		auto fh = ::file_try_get(name);
		EXPECT_FALSE(fh);
	}
	{
		auto temp_file = file_create_temp(name, 1024);
		EXPECT_TRUE(temp_file);
		auto fh = file_temp_to_final(&temp_file);
		EXPECT_FALSE(temp_file);
		EXPECT_TRUE(fh);
	}
	{
		auto fh0 = ::file_try_get(name);
		EXPECT_TRUE(fh0);
		auto fh1 = ::file_try_get(name);
		EXPECT_TRUE(fh1);
		EXPECT_EQ(fh0, fh1);
	}
}

TEST_F(file, open_file_three_times)
{
	const char name[] = "open_file_three_times";
	{
		auto fh = ::file_try_get(name);
		EXPECT_FALSE(fh);
		auto temp_file = file_create_temp(name, 1024);
		EXPECT_TRUE(temp_file);
		fh = file_temp_to_final(&temp_file);
		EXPECT_FALSE(temp_file);
		EXPECT_TRUE(fh);
	}
	{
		auto fh0 = ::file_try_get(name);
		EXPECT_TRUE(fh0);
		auto fh1 = ::file_try_get(name);
		EXPECT_TRUE(fh1);
		auto fh2 = ::file_try_get(name);
		EXPECT_TRUE(fh2);
		EXPECT_EQ(fh0, fh1);
		EXPECT_EQ(fh1, fh2);
		EXPECT_EQ(fh2, fh0);
	}
}

TEST_F(file, file_temp_to_final_return_is_new)
{
	const char name[] = "file_temp_to_final_return_is_new";
	auto temp_file = ::file_create_temp(name, 1024);
	EXPECT_TRUE(temp_file);
	auto final_file = file_temp_to_final(&temp_file);
	EXPECT_FALSE(temp_file);
	EXPECT_TRUE(final_file);
	EXPECT_NE(temp_file, final_file);
}

TEST_F(file, temp_file_size_0_exits)
{
	const char name[] = "temp_file_size_0";
	ASSERT_DEATH(::file_create_temp(name, 0), ".*");
}

TEST_F(file, temp_file_size_1)
{
	const char name[] = "temp_file_size_1";
	auto temp_file = ::file_create_temp(name, 1);
	EXPECT_TRUE(temp_file);
	EXPECT_EQ(1, file_get_size(temp_file));
	auto final_file = file_temp_to_final(&temp_file);
	EXPECT_FALSE(temp_file);
	EXPECT_TRUE(final_file);
	EXPECT_EQ(1, file_get_size(final_file));
	EXPECT_NE(temp_file, final_file);
}

TEST_F(file, temp_file_size_1KB)
{
	const char name[] = "temp_file_size_1KB";
	auto temp_file = ::file_create_temp(name, 1024);
	EXPECT_TRUE(temp_file);
	EXPECT_EQ(1024, file_get_size(temp_file));
	auto final_file = file_temp_to_final(&temp_file);
	EXPECT_FALSE(temp_file);
	EXPECT_TRUE(final_file);
	EXPECT_EQ(1024, file_get_size(final_file));
	EXPECT_NE(temp_file, final_file);
}

TEST_F(file, temp_file_size_1MB)
{
	const char name[] = "temp_file_size_1MB";
	auto temp_file = ::file_create_temp(name, 1024 * 1024);
	EXPECT_TRUE(temp_file);
	EXPECT_EQ(1024 * 1024, file_get_size(temp_file));
	auto final_file = file_temp_to_final(&temp_file);
	EXPECT_FALSE(temp_file);
	EXPECT_TRUE(final_file);
	EXPECT_EQ(1024 * 1024, file_get_size(final_file));
	EXPECT_NE(temp_file, final_file);
}

TEST_F(file, two_temp_file)
{
	const char name[] = "two_temp_file";
	auto temp_file_0 = ::file_create_temp(name, 1024);
	EXPECT_TRUE(temp_file_0);
	auto temp_file_1 = ::file_create_temp(name, 1024);
	EXPECT_TRUE(temp_file_1);

	auto final_file_0 = file_temp_to_final(&temp_file_0);
	EXPECT_FALSE(temp_file_0);
	EXPECT_TRUE(temp_file_1);
	EXPECT_TRUE(final_file_0);

	auto final_file_1 = file_temp_to_final(&temp_file_1);
	EXPECT_FALSE(temp_file_0);
	EXPECT_FALSE(temp_file_1);
	EXPECT_TRUE(final_file_1);

	EXPECT_EQ(final_file_0, final_file_1);
}
