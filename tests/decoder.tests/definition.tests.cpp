// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "definition.hpp"
#include "file.hpp"
#include "gtest/gtest.h"

// Include tracing library definition for creating V2 format
#include "definition/definition.h"

using namespace CommonLowLevelTracingKit::decoder::source;

class DecoderDefinitionTest : public ::testing::Test
{
  protected:
	std::string m_file_name;
	std::vector<uint8_t> m_buffer;

	void SetUp() override
	{
		// Use random number to avoid collisions when tests run in parallel processes
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
		m_file_name = "_definition_test_" + std::to_string(dist(gen)) + ".bin";
		if (std::filesystem::exists(m_file_name)) { std::filesystem::remove(m_file_name); }
		m_buffer.resize(1024, 0);
	}

	void TearDown() override
	{
		if (std::filesystem::exists(m_file_name)) { std::filesystem::remove(m_file_name); }
	}

	// Write buffer to file and return FilePart
	FilePart writeAndOpen(size_t size)
	{
		std::ofstream outFile(m_file_name, std::ios::binary | std::ios::trunc);
		outFile.write(reinterpret_cast<const char *>(m_buffer.data()),
					  static_cast<std::streamsize>(size));
		outFile.close();
		return FilePart(m_file_name);
	}

	// Create V1 format definition (legacy, no extended)
	size_t createV1Definition(const char *name)
	{
		size_t name_len = strlen(name);
		uint64_t body_size = name_len + 1;

		// Write header
		memcpy(m_buffer.data(), &body_size, sizeof(body_size));
		// Write name
		memcpy(m_buffer.data() + sizeof(uint64_t), name, name_len);
		m_buffer[sizeof(uint64_t) + name_len] = '\0';

		return sizeof(uint64_t) + body_size;
	}

	// Create V2 format definition using tracing library
	size_t createV2Definition(const char *name, definition_source_type_t source_type)
	{
		size_t name_len = strlen(name);
		size_t total_size = definition_calculate_size(name_len);
		definition_init(m_buffer.data(), name, name_len, source_type);
		return total_size;
	}
};

// ============================================================================
// V2 Format Parsing Tests
// ============================================================================

TEST_F(DecoderDefinitionTest, parse_v2_userspace)
{
	size_t size = createV2Definition("test_buffer", DEFINITION_SOURCE_USERSPACE);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), "test_buffer");
	EXPECT_TRUE(def.hasExtended());
	EXPECT_TRUE(def.isCrcValid());
	EXPECT_EQ(def.sourceType(), DefinitionSourceType::Userspace);
}

TEST_F(DecoderDefinitionTest, parse_v2_kernel)
{
	size_t size = createV2Definition("kernel_trace", DEFINITION_SOURCE_KERNEL);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), "kernel_trace");
	EXPECT_EQ(def.sourceType(), DefinitionSourceType::Kernel);
}

TEST_F(DecoderDefinitionTest, parse_v2_tty)
{
	size_t size = createV2Definition("TTY", DEFINITION_SOURCE_TTY);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), "TTY");
	EXPECT_EQ(def.sourceType(), DefinitionSourceType::TTY);
}

TEST_F(DecoderDefinitionTest, parse_v2_unknown)
{
	size_t size = createV2Definition("unknown_source", DEFINITION_SOURCE_UNKNOWN);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), "unknown_source");
	EXPECT_EQ(def.sourceType(), DefinitionSourceType::Unknown);
}

// ============================================================================
// V1 Format (Legacy) Parsing Tests
// ============================================================================

TEST_F(DecoderDefinitionTest, parse_v1_legacy)
{
	size_t size = createV1Definition("legacy_buffer");
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), "legacy_buffer");
	EXPECT_FALSE(def.hasExtended());
	EXPECT_TRUE(def.isCrcValid()); // V1 always returns true for CRC
	EXPECT_EQ(def.sourceType(), DefinitionSourceType::Unknown);
}

TEST_F(DecoderDefinitionTest, parse_v1_short_name)
{
	size_t size = createV1Definition("a");
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), "a");
	EXPECT_FALSE(def.hasExtended());
}

// ============================================================================
// CRC Validation Tests
// ============================================================================

TEST_F(DecoderDefinitionTest, crc_valid_all_source_types)
{
	definition_source_type_t types[] = {DEFINITION_SOURCE_UNKNOWN, DEFINITION_SOURCE_USERSPACE,
										DEFINITION_SOURCE_KERNEL, DEFINITION_SOURCE_TTY};

	int index = 0;
	for (auto source_type : types) {
		// Use unique file name for each iteration to avoid file locking issues
		std::string iter_file = "_crc_test_" + std::to_string(index++) + ".bin";

		memset(m_buffer.data(), 0, m_buffer.size());
		size_t size = createV2Definition("crc_test", source_type);

		// Write to unique file
		{
			std::ofstream outFile(iter_file, std::ios::binary | std::ios::trunc);
			outFile.write(reinterpret_cast<const char *>(m_buffer.data()),
						  static_cast<std::streamsize>(size));
		}

		FilePart file(iter_file);
		Definition def(file.get<FilePart>(0));

		EXPECT_TRUE(def.isCrcValid())
			<< "CRC should be valid for source_type=" << static_cast<int>(source_type);

		// Clean up
		std::filesystem::remove(iter_file);
	}
}

TEST_F(DecoderDefinitionTest, crc_invalid_corrupted_name)
{
	size_t size = createV2Definition("test_buffer", DEFINITION_SOURCE_USERSPACE);

	// Corrupt a byte in the name (after header, before extended)
	m_buffer[sizeof(uint64_t) + 2] = 'X';

	auto file = writeAndOpen(size);
	Definition def(file.get<FilePart>(0));

	EXPECT_FALSE(def.isCrcValid());
	EXPECT_FALSE(def.isValid()); // isValid checks CRC for extended format
}

TEST_F(DecoderDefinitionTest, crc_invalid_corrupted_source_type)
{
	const char *name = "test";
	size_t size = createV2Definition(name, DEFINITION_SOURCE_USERSPACE);

	// Corrupt source_type field (offset: header + name + null + magic + version)
	size_t source_offset = sizeof(uint64_t) + strlen(name) + 1 + DEFINITION_EXTENDED_MAGIC_SIZE + 1;
	m_buffer[source_offset] = 0xFF;

	auto file = writeAndOpen(size);
	Definition def(file.get<FilePart>(0));

	EXPECT_FALSE(def.isCrcValid());
}

TEST_F(DecoderDefinitionTest, crc_invalid_corrupted_crc_byte)
{
	const char *name = "test";
	size_t size = createV2Definition(name, DEFINITION_SOURCE_USERSPACE);

	// Corrupt the CRC byte itself (last byte of extended)
	size_t crc_offset = sizeof(uint64_t) + strlen(name) + 1 + sizeof(definition_extended_t) - 1;
	m_buffer[crc_offset] ^= 0xFF;

	auto file = writeAndOpen(size);
	Definition def(file.get<FilePart>(0));

	EXPECT_FALSE(def.isCrcValid());
}

// ============================================================================
// Extended Format Detection Tests
// ============================================================================

TEST_F(DecoderDefinitionTest, has_extended_v2)
{
	size_t size = createV2Definition("test", DEFINITION_SOURCE_USERSPACE);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));
	EXPECT_TRUE(def.hasExtended());
}

TEST_F(DecoderDefinitionTest, has_extended_v1)
{
	size_t size = createV1Definition("test");
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));
	EXPECT_FALSE(def.hasExtended());
}

TEST_F(DecoderDefinitionTest, has_extended_corrupted_magic)
{
	size_t size = createV2Definition("test", DEFINITION_SOURCE_USERSPACE);

	// Corrupt the magic bytes
	size_t magic_offset = sizeof(uint64_t) + strlen("test") + 1;
	m_buffer[magic_offset] = 'X';

	auto file = writeAndOpen(size);
	Definition def(file.get<FilePart>(0));

	EXPECT_FALSE(def.hasExtended());
	EXPECT_EQ(def.sourceType(), DefinitionSourceType::Unknown);
}

// ============================================================================
// Various Name Tests
// ============================================================================

TEST_F(DecoderDefinitionTest, parse_long_name)
{
	std::string long_name(200, 'x');
	size_t size = createV2Definition(long_name.c_str(), DEFINITION_SOURCE_KERNEL);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), long_name);
}

TEST_F(DecoderDefinitionTest, parse_name_with_path)
{
	const char *name = "/path/to/tracebuffer.clltk_trace";
	size_t size = createV2Definition(name, DEFINITION_SOURCE_USERSPACE);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), name);
}

TEST_F(DecoderDefinitionTest, parse_name_with_special_chars)
{
	const char *name = "trace-buffer_123.test";
	size_t size = createV2Definition(name, DEFINITION_SOURCE_USERSPACE);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), name);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DecoderDefinitionTest, minimum_valid_name)
{
	size_t size = createV2Definition("x", DEFINITION_SOURCE_USERSPACE);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), "x");
}

TEST_F(DecoderDefinitionTest, empty_body_size)
{
	// Create invalid definition with body_size = 0
	uint64_t body_size = 0;
	memcpy(m_buffer.data(), &body_size, sizeof(body_size));

	auto file = writeAndOpen(sizeof(uint64_t));
	Definition def(file.get<FilePart>(0));

	// Should not crash, but may be invalid
	EXPECT_FALSE(def.isValid());
}

TEST_F(DecoderDefinitionTest, body_size_larger_than_file)
{
	// Create definition with body_size claiming more data than available
	uint64_t body_size = 10000;
	memcpy(m_buffer.data(), &body_size, sizeof(body_size));
	m_buffer[sizeof(uint64_t)] = 't';
	m_buffer[sizeof(uint64_t) + 1] = '\0';

	// Write only a small portion
	auto file = writeAndOpen(20);
	Definition def(file.get<FilePart>(0));

	// Should handle gracefully
	EXPECT_FALSE(def.isValid());
}

// ============================================================================
// Roundtrip Tests (Write with tracing lib, read with decoder)
// ============================================================================

class DecoderDefinitionRoundtripTest
	: public DecoderDefinitionTest,
	  public ::testing::WithParamInterface<
		  std::tuple<std::string, definition_source_type_t, DefinitionSourceType>>
{
};

TEST_P(DecoderDefinitionRoundtripTest, roundtrip)
{
	const auto &[name, write_type, expected_read_type] = GetParam();

	size_t size = createV2Definition(name.c_str(), write_type);
	auto file = writeAndOpen(size);

	Definition def(file.get<FilePart>(0));

	EXPECT_TRUE(def.isValid());
	EXPECT_EQ(def.name(), name);
	EXPECT_TRUE(def.hasExtended());
	EXPECT_TRUE(def.isCrcValid());
	EXPECT_EQ(def.sourceType(), expected_read_type);
}

INSTANTIATE_TEST_SUITE_P(
	RoundtripTests, DecoderDefinitionRoundtripTest,
	::testing::Values(
		std::make_tuple("test", DEFINITION_SOURCE_UNKNOWN, DefinitionSourceType::Unknown),
		std::make_tuple("userspace_buffer", DEFINITION_SOURCE_USERSPACE,
						DefinitionSourceType::Userspace),
		std::make_tuple("kernel_module", DEFINITION_SOURCE_KERNEL, DefinitionSourceType::Kernel),
		std::make_tuple("TTY", DEFINITION_SOURCE_TTY, DefinitionSourceType::TTY),
		std::make_tuple("complex/path/to/buffer.trace", DEFINITION_SOURCE_USERSPACE,
						DefinitionSourceType::Userspace),
		std::make_tuple("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", DEFINITION_SOURCE_KERNEL,
						DefinitionSourceType::Kernel)));
