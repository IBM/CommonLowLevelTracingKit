// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <array>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <stdint.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

#include "file.hpp"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit;
using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::source;
using namespace std::string_literals;

class decoder_file_part : public ::testing::Test
{
  protected:
	std::string m_file_name;
	std::string m_tracebuffer_name;
	std::array<unsigned char, 4096> m_data;
	void SetUp() override
	{
		constinit static int file_index = 0;
		m_tracebuffer_name = "_test_" + std::to_string(file_index++);
		m_file_name = m_tracebuffer_name + ".clltk_trace";
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
};

TEST_F(decoder_file_part, empty_file)
{
	std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
	const auto file = FilePart(m_file_name);
}

TEST_F(decoder_file_part, path)
{
	std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
	const auto file = FilePart(m_file_name);
	EXPECT_EQ(file.path(), m_file_name);
}

TEST_F(decoder_file_part, get)
{
	std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
	outFile.write(reinterpret_cast<const char *>(m_data.data()), 256);
	outFile.close();
	const auto file = FilePart(m_file_name);
	ASSERT_EQ(file.get<uint8_t>(), *reinterpret_cast<uint8_t *>(m_data.data()));
	ASSERT_EQ(file.get<uint16_t>(), *reinterpret_cast<uint16_t *>(m_data.data()));
	ASSERT_EQ(file.get<uint32_t>(), *reinterpret_cast<uint32_t *>(m_data.data()));
	ASSERT_EQ(file.get<uint64_t>(), *reinterpret_cast<uint64_t *>(m_data.data()));

	EXPECT_ANY_THROW(file.get<uint8_t>(257));
}

TEST_F(decoder_file_part, getRef)
{
	std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
	outFile.write(reinterpret_cast<const char *>(m_data.data()), 256);
	outFile.close();
	const auto file = FilePart(m_file_name);
	ASSERT_EQ(file.getRef<uint8_t>(), *reinterpret_cast<uint8_t *>(m_data.data()));
	ASSERT_EQ(file.getRef<uint16_t>(), *reinterpret_cast<uint16_t *>(m_data.data()));
	ASSERT_EQ(file.getRef<uint32_t>(), *reinterpret_cast<uint32_t *>(m_data.data()));
	ASSERT_EQ(file.getRef<uint64_t>(), *reinterpret_cast<uint64_t *>(m_data.data()));

	EXPECT_ANY_THROW(file.get<uint8_t>(257));
}

TEST_F(decoder_file_part, getFilePart)
{
	std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
	outFile.write(reinterpret_cast<const char *>(m_data.data()), 256);
	outFile.close();
	const auto file = FilePart(m_file_name);
	const auto subFile = file.get<FilePart>(1);
	ASSERT_EQ(subFile.getRef<uint8_t>(), *reinterpret_cast<uint8_t *>(m_data.data() + 1));
	ASSERT_EQ(subFile.getRef<uint16_t>(), *reinterpret_cast<uint16_t *>(m_data.data() + 1));
	ASSERT_EQ(subFile.getRef<uint32_t>(), *reinterpret_cast<uint32_t *>(m_data.data() + 1));
	ASSERT_EQ(subFile.getRef<uint64_t>(), *reinterpret_cast<uint64_t *>(m_data.data() + 1));
	ASSERT_EQ(&file.getRef<uint8_t>(10), &subFile.getRef<uint8_t>(10 - 1));
}

TEST_F(decoder_file_part, grow_auto)
{
	std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
	outFile
		.write(reinterpret_cast<const char *>(m_data.data()),
			   static_cast<std::streamsize>(m_data.size()))
		.flush();
	auto file = FilePart(m_file_name);
	ASSERT_EQ(file.getFileSize(), m_data.size());
	outFile
		.write(reinterpret_cast<const char *>(m_data.data()),
			   static_cast<std::streamsize>(m_data.size()))
		.flush();
	ASSERT_EQ(file.getFileSize(), m_data.size());
	ASSERT_EQ(file.get<uint8_t>(m_data.size()), m_data.at(0));
	ASSERT_EQ(file.getFileSize(), 2 * m_data.size());
}
TEST_F(decoder_file_part, grow)
{
	std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
	outFile.write(reinterpret_cast<const char *>(m_data.data()), 1).flush();
	auto file = FilePart(m_file_name);
	ASSERT_EQ(file.getFileSize(), 1);
	outFile.write(reinterpret_cast<const char *>(m_data.data()), 1).flush();
	ASSERT_EQ(file.getFileSize(), 1);
	file.doGrow();
	ASSERT_EQ(file.getFileSize(), 2);
}

TEST_F(decoder_file_part, mmap)
{

	{
		std::ofstream outFile(m_file_name, std::ios::binary | std::ios::app);
		ASSERT_TRUE(outFile);
		outFile.write(reinterpret_cast<const char *>(m_data.data()),
					  static_cast<std::streamsize>(m_data.size()));
		outFile.close();
	}
	int fd = open(m_file_name.c_str(), O_RDWR);
	ASSERT_GT(fd, 0);
	uint8_t *const ptr =
		(uint8_t *)mmap(nullptr, m_data.size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	EXPECT_NE(ptr, MAP_FAILED);
	ptr[0] = 1;
	ptr[m_data.size() - 1] = 2;

	auto file = FilePart(m_file_name);
	EXPECT_NE(&file.getRef<uint8_t>(), ptr);
	EXPECT_EQ(file.get<uint8_t>(0), 1);
	EXPECT_EQ(file.get<uint8_t>(m_data.size() - 1), 2);
	munmap(ptr, m_data.size());
	close(fd);
}
TEST_F(decoder_file_part, getLimted)
{

	static constexpr uint64_t data = 0x123456789ABCDEFULL;
	static constexpr uint64_t data_r =
		(0xFFFFFFFF & (data >> (4 * 8))) + ((0xFFFFFFFF & data) << (4 * 8));

	{
		std::ofstream outFile(m_file_name, std::ios::binary | std::ios::app);
		ASSERT_TRUE(outFile);
		outFile.write(reinterpret_cast<const char *>(&data_r),
					  static_cast<std::streamsize>(sizeof(data_r)));
	}

	auto file = FilePart(m_file_name);
	const uint64_t v_r = file.getLimted<uint64_t>(sizeof(uint64_t), 0);
	EXPECT_EQ(v_r, data_r);
	const uint64_t v = file.getLimted<uint64_t>(sizeof(uint64_t), sizeof(data) / 2);
	EXPECT_EQ(v, data) << "0x" << std::setw(sizeof(data) * 2) << std::hex << v;
}