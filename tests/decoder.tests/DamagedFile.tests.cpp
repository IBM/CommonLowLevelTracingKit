// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "helper.hpp"
#include "gtest/gtest.h"

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_DamagedFile)
constexpr size_t TB_SIZE = 64;
CLLTK_TRACEBUFFER(TB, TB_SIZE)

using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::exception;
using namespace std::string_literals;
using namespace std::chrono;
class decoder_DamagedFile : public ::testing::Test
{
  protected:
	std::string m_file_name;
	std::string m_tb_name;
	void SetUp() override
	{
		m_tb_name = STR(TB);
		m_file_name = trace_file(m_tb_name);
		SETUP(TB);
	}
	void TearDown() override { CLEANUP(TB); }
};

static std::string trace()
{ // tracepoint in tracebuffer
	uint8_t arg0 = 126;
	int16_t arg1 = -512;
	uint16_t arg2 = 1024;
	double arg3 = 3e-10;
	float arg4 = 9.9e-30F;
	char arg5[] = "ABC~{\0FG";
	uint16_t arg6 = 10000;
	int16_t arg7 = -10000;
	double arg8 = -1.11;
	void *arg9 = std::bit_cast<void *>(42lu);
	TP(" %u %o %x %g %f %s %X %d %e %p", arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,
	   arg9);
	char *expect_raw = nullptr;
	EXPECT_GT(asprintf(&expect_raw, " %u %o %x %g %f %s %X %d %e %p", arg0, arg1, arg2, arg3, arg4,
					   arg5, arg6, arg7, arg8, arg9),
			  10);
	EXPECT_TRUE(expect_raw);
	std::string expect = std::string(expect_raw);
	free(expect_raw);
	return expect;
};
std::tuple<FILE *, size_t> open(const char *str)
{
	auto f = fopen(str, "r+");
	fseek(f, 0, SEEK_END);
	const size_t fileSize = (size_t)ftell(f);
	rewind(f);
	EXPECT_LT(fileSize, 1024);
	return {f, fileSize};
}

struct Changed {
	FILE *f;
	size_t offset;
	uint16_t old;
	Changed(FILE *a_f, size_t a_offset)
		: f(a_f)
		, offset(a_offset)
		, old(0)
	{
		EXPECT_EQ(pread(fileno(f), &old, 1, (long)offset), 1) << strerror(errno);
	}
	void change(uint16_t damaged)
	{
		EXPECT_EQ(pwrite(fileno(f), &damaged, 1, (long)offset), 1) << strerror(errno);
	}
	~Changed() { EXPECT_EQ(pwrite(fileno(f), &old, 1, (long)offset), 1) << strerror(errno); }
};

TEST_F(decoder_DamagedFile, get_msg_damages)
{
	const std::string expect = trace();
	auto [f, fileSize] = open(m_file_name.c_str());
	size_t printed = 0;
	for (size_t offset = 0; offset < fileSize; offset++) {
		Changed c{f, offset};
		uint16_t damaged = static_cast<uint16_t>(std::rand());
		if (damaged == c.old) continue;
		c.change(damaged);
		auto tb = SyncTracebuffer::make(m_file_name);
		if (!tb) continue;
		auto tp = tb->next();
		if (!tp) continue;
		try {
			std::ignore = tp->msg();
		} catch (...) {
		}
		if ((offset * 100 / fileSize) >= (printed + 5)) {
			std::cout << "|" << std::flush;
			printed = (offset * 100 / fileSize);
		}
	}
	std::cout << std::endl;
}