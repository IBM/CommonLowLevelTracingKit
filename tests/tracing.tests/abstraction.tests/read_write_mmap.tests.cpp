// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/file.h"
#include "gtest/gtest.h"
#include <array>
#include <cctype>
#include <cstdlib>
#include <stdint.h>
#include <string.h>
#include <string>

static_assert((__cplusplus == 202002L), "not c++17");

class read_write_mmap : public ::testing::Test
{

  protected:
	void SetUp() override { file_reset(); }
};

std::string create_string(const size_t size)
{
	std::string str(size, 0);
	for (auto &c : str) {
		while (!std::isalnum(c = static_cast<char>(std::rand())))
			;
	}
	return str;
}

TEST_F(read_write_mmap, write_read)
{
	std::array<uint8_t, 1024> data{};
	auto str = create_string(data.size());

	file_t *fh = file_create_temp("file name", data.size());

	EXPECT_EQ(str.size(), file_pwrite(fh, str.data(), str.size(), 0))
		<< "could not write the right number of bytes";
	EXPECT_EQ(data.size(), file_pread(fh, data.data(), data.size(), 0))
		<< "could not read the right number of bytes";

	EXPECT_EQ(std::string(data.begin(), data.end()), str) << "read back from file failed";
	file_drop(&fh);
}

TEST_F(read_write_mmap, mmap_read)
{
	std::string data(1024, 0);
	auto str = create_string(data.size());

	file_t *fh = file_create_temp("file name", data.size());

	auto ptr = file_mmap_ptr(fh);
	EXPECT_NE(ptr, nullptr) << "mmap failed";
	memcpy(ptr, str.c_str(), str.size());

	EXPECT_EQ(data.size(), file_pread(fh, data.data(), data.size(), 0))
		<< "could not read the right number of bytes";

	EXPECT_EQ(std::string(data.begin(), data.end()), str) << "read back from file failed";
	file_drop(&fh);
}

TEST_F(read_write_mmap, write_mmap)
{
	std::string data(1024, 0);
	auto str = create_string(data.size());

	file_t *fh = file_create_temp("file name", data.size());

	auto ptr = file_mmap_ptr(fh);
	EXPECT_NE(ptr, nullptr) << "mmap failed";

	EXPECT_EQ(str.size(), file_pwrite(fh, str.data(), str.size(), 0))
		<< "could not write the right number of bytes";

	EXPECT_EQ(str, std::string(static_cast<const char *>(ptr), data.size()))
		<< "read with mmap failed";
	file_drop(&fh);
}

TEST_F(read_write_mmap, write_more_than_file_size)
{
	auto str = create_string(1024);

	file_t *fh = file_create_temp("file name", str.size() - 1);
	EXPECT_EQ(str.size(), file_pwrite(fh, str.data(), str.size(), 0));
	EXPECT_EQ(str.size(), file_get_size(fh));
	file_drop(&fh);
}