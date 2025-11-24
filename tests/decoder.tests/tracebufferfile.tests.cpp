// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <array>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <random>
#include <stdint.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "definition.hpp"
#include "file.hpp"
#include "helper.hpp"
#include "ringbuffer.hpp"
#include "tracebufferfile.hpp"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit;
using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::source;
using namespace std::string_literals;
class decoder_tracebufferfile : public ::testing::Test
{
  protected:
	std::string m_file_name;
	std::string m_tracebuffer_name;
	std::array<unsigned char, 4096> m_data;
	void SetUp() override
	{
		constinit static int file_index = 0;
		m_tracebuffer_name = "_decoder_tracebufferfile_test_" + std::to_string(file_index++);
		m_file_name = trace_file(m_tracebuffer_name);
		if (std::filesystem::exists(m_file_name)) { std::filesystem::remove(m_file_name); }
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dist(0, 255);
		for (auto &byte : m_data) { byte = static_cast<unsigned char>(dist(gen)); }
	}
	void TearDown() override
	{
		if (std::filesystem::exists(m_file_name)) { std::filesystem::remove(m_file_name); }
	}
	void write_to_file(const void *data, size_t size, size_t offset)
	{
		const int fd = open(m_file_name.c_str(), O_WRONLY);
		EXPECT_GT(fd, 3);
		EXPECT_EQ(pwrite(fd, data, size, (ssize_t)offset), size) << strerror(errno);
	}
	template <typename T> void write_to_file(T data, size_t offset)
	{
		write_to_file(&data, sizeof(T), offset);
	}
	void read_from_file(void *data, size_t size, size_t offset)
	{
		const int fd = open(m_file_name.c_str(), O_RDONLY);
		EXPECT_GT(fd, 3);
		EXPECT_EQ(pread(fd, data, size, (ssize_t)offset), size) << strerror(errno);
	}
};
TEST_F(decoder_tracebufferfile, valid_header)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 1024);
	TracebufferFile tb{m_file_name};

	for (size_t offset = 0; offset < 56; offset++) {
		uint8_t oldValue = 0;
		read_from_file(&oldValue, sizeof(oldValue), offset);
		write_to_file<uint8_t>(oldValue + 1, offset);
		if (offset < 56)
			EXPECT_ANY_THROW(TracebufferFile{m_file_name});
		else
			EXPECT_NO_THROW(TracebufferFile{m_file_name});
		write_to_file(oldValue, offset);
		EXPECT_NO_THROW(TracebufferFile{m_file_name});
	}
}
TEST_F(decoder_tracebufferfile, version)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 1024);
	TracebufferFile tb{m_file_name};
	using VersionType = std::array<char, 8>;
	const auto rawVersion = tb.getFilePart().getReference<VersionType>(16);
	auto [major, minor, path] = tb.getVersion();
	EXPECT_EQ(major, rawVersion.at(2));
	EXPECT_EQ(minor, rawVersion.at(1));
	EXPECT_EQ(path, rawVersion.at(0));
}
TEST_F(decoder_tracebufferfile, definition)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 1024);
	TracebufferFile tb{m_file_name};
	auto &definition = tb.getDefinition();
	EXPECT_EQ(definition.name(), m_tracebuffer_name);
	const std::string def_str{definition.name()};
	EXPECT_EQ(def_str, m_tracebuffer_name);
}
TEST_F(decoder_tracebufferfile, ringbuffer_version)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 1024);
	TracebufferFile tb{m_file_name};
	EXPECT_EQ(tb.getRingbuffer().getVersion(), 1);
}
TEST_F(decoder_tracebufferfile, ringbuffer_body_size)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 1024);
	TracebufferFile tb{m_file_name};
	EXPECT_GE(tb.getRingbuffer().getSize(), 1024);
	EXPECT_LT(tb.getRingbuffer().getSize(), 1024 * 2);
}

TEST_F(decoder_tracebufferfile, ringbuffer_wrapped_dropped_and_entries)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 1024);
	TracebufferFile tb{m_file_name};
	EXPECT_EQ(tb.getRingbuffer().getWrapped(), 0);
	clltk_dynamic_tracepoint_execution(m_tracebuffer_name.c_str(), __FILE__, __LINE__, 0, 0,
									   "Hello World %d", 0);
	EXPECT_EQ(tb.getRingbuffer().getWrapped(), 0);
	for (uint loop_index = 1; loop_index < 100; loop_index++) {
		clltk_dynamic_tracepoint_execution(m_tracebuffer_name.c_str(), __FILE__, __LINE__, 0, 0,
										   "Hello World %d", loop_index);
	}
	EXPECT_GT(tb.getRingbuffer().getWrapped(), 0);
	const auto dropped = tb.getRingbuffer().getDropped();
	EXPECT_GT(dropped, 0);
	const auto entryCount = tb.getRingbuffer().getEntryCount();
	EXPECT_EQ(entryCount, 100);
	clltk_dynamic_tracepoint_execution(m_tracebuffer_name.c_str(), __FILE__, __LINE__, 0, 0,
									   "Hello World %d", 100);
	EXPECT_EQ(dropped + 1, tb.getRingbuffer().getDropped());
	EXPECT_EQ(entryCount + 1, tb.getRingbuffer().getEntryCount());
}