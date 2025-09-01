// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/file.h"
#include "gtest/gtest.h"
#include <array>
#include <stddef.h>
#include <string>
#include <string_view>

class file_utilities : public ::testing::Test
{
  protected:
	void SetUp() override { file_reset(); }
};

class file_pwrite : public file_utilities
{
};
class file_pread : public file_utilities
{
};

TEST_F(file_pwrite, write)
{
	file_t *fd = file_create_temp("file_name", 1024);
	std::string str0 = "ABCD";
	std::string str1 = "DEFG";
	::file_pwrite(fd, str0.c_str(), str0.size(), 0);
	::file_pwrite(fd, str1.c_str(), str1.size(), str0.size() - 1);

	std::array<char, 3 + 1 + 3> data = {};
	file_pread(fd, data.data(), data.size(), 0);
	using namespace std::string_literals;
	EXPECT_EQ(std::string(data.data(), data.size()), "ABCDEFG"s);
	file_drop(&fd);
}

TEST_F(file_pwrite, write_after_file_end)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	std::string str = "ABC";
	::file_pwrite(fd, str.c_str(), str.size(), file_size);

	std::array<char, 3> data = {};
	file_pread(fd, data.data(), data.size(), file_size);
	EXPECT_EQ(std::string(data.data(), data.size()), str);
	file_drop(&fd);
}

TEST_F(file_pwrite, write_at_file_end)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	std::string str = "ABC";
	::file_pwrite(fd, str.c_str(), str.size(), file_size - 2);

	std::array<char, 3> data = {};
	file_pread(fd, data.data(), data.size(), file_size - 2);
	EXPECT_EQ(std::string(data.data(), data.size()), str);
	file_drop(&fd);
}

TEST_F(file_pread, read)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	std::string_view str = "ABCDEF";
	file_pwrite(fd, str.data(), str.size(), 0);

	{
		std::array<char, 3> data{};
		::file_pread(fd, data.data(), data.size(), 0);
		using namespace std::string_literals;
		EXPECT_EQ(std::string(data.data(), data.size()), "ABC"s);
	}
	{
		std::array<char, 3> data{};
		::file_pread(fd, data.data(), data.size(), 3);
		using namespace std::string_literals;
		EXPECT_EQ(std::string(data.data(), data.size()), "DEF"s);
	}
	file_drop(&fd);
}
TEST_F(file_pread, read_at_file_end)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);

	std::array<char, 32> data{};
	EXPECT_EXIT(::file_pread(fd, data.data(), data.size(), file_size - (data.size() / 2)),
				::testing::ExitedWithCode(1), "clltk unrecoverable");
	file_drop(&fd);
}

TEST_F(file_pread, read_after_file_end)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);

	std::array<char, 32> data{};

	EXPECT_EXIT(::file_pread(fd, data.data(), data.size(), file_size), ::testing::ExitedWithCode(1),
				"clltk unrecoverable");
	file_drop(&fd);
}
