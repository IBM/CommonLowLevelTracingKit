// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "unique_stack/unique_stack.h"
#include "abstraction/file.h"
#include "gtest/gtest.h"
#include <array>
#include <iomanip>
#include <ostream>
#include <stdint.h>
#include <string>
#include <unistd.h>

class unique_stack_init : public testing::Test
{
  public:
	static inline const char *root = ".";

  protected:
	void SetUp() override { file_reset(); }
	void TearDown() override { file_reset(); }
};

TEST_F(unique_stack_init, simple)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	EXPECT_TRUE(unique_stack_valid(&uq));
	unique_stack_close(&uq);
	file_drop(&fd);
}

class unique_stack_open : public unique_stack_init
{
};

TEST_F(unique_stack_open, simple)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	// init stack
	{
		unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
		unique_stack_close(&uq);
	}

	// open stack
	{
		unique_stack_handler_t uq = ::unique_stack_open(fd, 0);
		EXPECT_TRUE(unique_stack_valid(&uq));
	}
	file_drop(&fd);
}

TEST_F(unique_stack_open, invalid_file)
{
	unique_stack_handler_t uq = ::unique_stack_init(nullptr, 0);
	EXPECT_FALSE(unique_stack_valid(&uq));
}

extern "C" {
struct file_t_coppied {
	uint64_t used;
	int file_descriptor;
};
}

TEST_F(unique_stack_open, bad_file_descriptor)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	const struct file_t_coppied *const f = (struct file_t_coppied *)fd;
	close(f->file_descriptor);
	EXPECT_EXIT(::unique_stack_init(fd, 0), ::testing::ExitedWithCode(1),
				"clltk unrecoverable: pwrite failed ");
	file_drop(&fd);
}

class unique_stack_close : public unique_stack_init
{
};

TEST_F(unique_stack_close, simple)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	::unique_stack_close(&uq);
	EXPECT_FALSE(unique_stack_valid(&uq));
	file_drop(&fd);
}

class unique_stack_add : public unique_stack_init
{
};

TEST_F(unique_stack_add, simple)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	std::string in = "A B C D E F G";
	uint64_t id = ::unique_stack_add(&uq, in.data(), (uint32_t)in.size());
	EXPECT_GT(id, 0);
	file_drop(&fd);
}

TEST_F(unique_stack_add, bad_file_descriptor)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	// const struct file_t_coppied * const f = (struct file_t_coppied*)fd;
	std::string in = "A B C D E F G";
	uint64_t id = ::unique_stack_add(&uq, in.data(), (uint32_t)in.size());
	EXPECT_GT(id, 0);
	file_drop(&fd);
}

TEST_F(unique_stack_add, bigger_than_file)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	std::array<uint8_t, file_size + 1> in = {};
	uint64_t id = ::unique_stack_add(&uq, in.data(), (uint32_t)in.size());
	EXPECT_GT(id, 0);
	EXPECT_GT(file_get_size(fd), file_size);
	file_drop(&fd);
}

TEST_F(unique_stack_add, twice_same_data0)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	std::string in = "A B C D E F G";
	uint64_t id0 = ::unique_stack_add(&uq, in.data(), (uint32_t)in.size());
	uint64_t id1 = ::unique_stack_add(&uq, in.data(), (uint32_t)in.size());
	EXPECT_GT(id0, 0);
	EXPECT_GT(id1, 0);
	EXPECT_EQ(id0, id1);
	file_drop(&fd);
}

TEST_F(unique_stack_add, twice_different_data0)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	std::string in0 = "A B C D E F G";
	uint64_t id0 = ::unique_stack_add(&uq, in0.data(), (uint32_t)in0.size());
	std::string in1 = "G F E D C B A";
	uint64_t id1 = ::unique_stack_add(&uq, in1.data(), (uint32_t)in1.size());
	EXPECT_NE(id0, id1);
	file_drop(&fd);
}

TEST_F(unique_stack_add, three_different_data0)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	std::string in0 = "A B C D E F G";
	uint64_t id0 = ::unique_stack_add(&uq, in0.data(), (uint32_t)in0.size());
	EXPECT_TRUE(id0);

	std::string in1 = "G F E D C B A";
	uint64_t id1 = ::unique_stack_add(&uq, in1.data(), (uint32_t)in1.size());
	EXPECT_TRUE(id1);
	EXPECT_NE(id0, id1);

	std::string in2 = "Z Y X";
	uint64_t id2 = ::unique_stack_add(&uq, in2.data(), (uint32_t)in2.size());
	EXPECT_TRUE(id2);
	EXPECT_NE(id0, id2);
	file_drop(&fd);
}

TEST_F(unique_stack_add, add_second_page)
{
	constexpr size_t file_size = 1024;
	file_t *fd = file_create_temp("file_name", file_size);
	unique_stack_handler_t uq = ::unique_stack_init(fd, 0);
	int i = 0;
	while ((size_t)getpagesize() > file_get_size(fd)) {
		std::ostringstream oss;
		oss << "<#>" << std::setw(6) << std::to_string(i++);
		std::string in = oss.str();
		uint64_t id = ::unique_stack_add(&uq, in.data(), (uint32_t)in.size() - 1);
		EXPECT_GT(id, 0);
	}
	EXPECT_GT(file_get_size(fd), getpagesize());
	file_drop(&fd);
}
