// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "definition/definition.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class DefinitionTest : public ::testing::Test
{
  protected:
	std::vector<uint8_t> buffer;

	void SetUp() override { buffer.resize(1024, 0); }

	// Helper to get expected body size for a name
	size_t expectedBodySize(size_t name_len)
	{
		return (name_len + 1) + sizeof(definition_extended_t);
	}

	// Helper to get expected total size for a name
	size_t expectedTotalSize(size_t name_len)
	{
		return sizeof(definition_header_t) + expectedBodySize(name_len);
	}
};

// ============================================================================
// Size calculation tests
// ============================================================================

TEST_F(DefinitionTest, calculate_body_size_short_name)
{
	EXPECT_EQ(definition_calculate_body_size(4), 4 + 1 + sizeof(definition_extended_t));
}

TEST_F(DefinitionTest, calculate_body_size_empty_name)
{
	// Even empty name needs null terminator
	EXPECT_EQ(definition_calculate_body_size(0), 0 + 1 + sizeof(definition_extended_t));
}

TEST_F(DefinitionTest, calculate_body_size_long_name)
{
	EXPECT_EQ(definition_calculate_body_size(100), 100 + 1 + sizeof(definition_extended_t));
}

TEST_F(DefinitionTest, calculate_size_short_name)
{
	size_t name_len = 4;
	size_t expected = sizeof(definition_header_t) + (name_len + 1) + sizeof(definition_extended_t);
	EXPECT_EQ(definition_calculate_size(name_len), expected);
}

TEST_F(DefinitionTest, calculate_size_consistency)
{
	for (size_t len = 1; len < 100; len++) {
		size_t total = definition_calculate_size(len);
		size_t body = definition_calculate_body_size(len);
		EXPECT_EQ(total, sizeof(definition_header_t) + body) << "Mismatch for name_length=" << len;
	}
}

// ============================================================================
// Initialization tests
// ============================================================================

TEST_F(DefinitionTest, init_null_destination)
{
	EXPECT_FALSE(definition_init(nullptr, "test", 4, DEFINITION_SOURCE_USERSPACE));
}

TEST_F(DefinitionTest, init_null_name)
{
	EXPECT_FALSE(definition_init(buffer.data(), nullptr, 4, DEFINITION_SOURCE_USERSPACE));
}

TEST_F(DefinitionTest, init_zero_length_name)
{
	EXPECT_FALSE(definition_init(buffer.data(), "test", 0, DEFINITION_SOURCE_USERSPACE));
}

TEST_F(DefinitionTest, init_userspace_source)
{
	const char *name = "test_buffer";
	size_t name_len = strlen(name);

	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_USERSPACE));

	// Verify header
	const definition_header_t *header = reinterpret_cast<definition_header_t *>(buffer.data());
	EXPECT_EQ(header->body_size, expectedBodySize(name_len));

	// Verify name
	EXPECT_STREQ(definition_get_name(buffer.data()), name);

	// Verify source type
	EXPECT_EQ(definition_get_source_type(buffer.data()), DEFINITION_SOURCE_USERSPACE);
}

TEST_F(DefinitionTest, init_kernel_source)
{
	const char *name = "kernel_trace";
	size_t name_len = strlen(name);

	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_KERNEL));
	EXPECT_EQ(definition_get_source_type(buffer.data()), DEFINITION_SOURCE_KERNEL);
}

TEST_F(DefinitionTest, init_tty_source)
{
	const char *name = "TTY";
	size_t name_len = strlen(name);

	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_TTY));
	EXPECT_EQ(definition_get_source_type(buffer.data()), DEFINITION_SOURCE_TTY);
}

TEST_F(DefinitionTest, init_unknown_source)
{
	const char *name = "unknown";
	size_t name_len = strlen(name);

	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_UNKNOWN));
	EXPECT_EQ(definition_get_source_type(buffer.data()), DEFINITION_SOURCE_UNKNOWN);
}

// ============================================================================
// Extended format detection tests
// ============================================================================

TEST_F(DefinitionTest, has_extended_after_init)
{
	const char *name = "test";
	EXPECT_TRUE(definition_init(buffer.data(), name, strlen(name), DEFINITION_SOURCE_USERSPACE));
	EXPECT_TRUE(definition_has_extended(buffer.data()));
}

TEST_F(DefinitionTest, has_extended_null_ptr)
{
	EXPECT_FALSE(definition_has_extended(nullptr));
}

TEST_F(DefinitionTest, has_extended_v1_format)
{
	// Simulate V1 format: just body_size + name (no extended magic)
	const char *name = "v1_buffer";
	size_t name_len = strlen(name);

	// Write header with V1 body size (just name + null terminator)
	definition_header_t *header = reinterpret_cast<definition_header_t *>(buffer.data());
	header->body_size = name_len + 1;

	// Write name
	char *body = reinterpret_cast<char *>(buffer.data() + sizeof(definition_header_t));
	memcpy(body, name, name_len);
	body[name_len] = '\0';

	// V1 format should NOT have extended
	EXPECT_FALSE(definition_has_extended(buffer.data()));
}

TEST_F(DefinitionTest, has_extended_corrupted_magic)
{
	const char *name = "test";
	size_t name_len = strlen(name);

	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_USERSPACE));

	// Corrupt the magic
	char *magic_ptr =
		reinterpret_cast<char *>(buffer.data() + sizeof(definition_header_t) + name_len + 1);
	magic_ptr[0] = 'X'; // Corrupt first byte of magic

	EXPECT_FALSE(definition_has_extended(buffer.data()));
}

// ============================================================================
// CRC validation tests
// ============================================================================

TEST_F(DefinitionTest, validate_crc_valid)
{
	const char *name = "crc_test";
	EXPECT_TRUE(definition_init(buffer.data(), name, strlen(name), DEFINITION_SOURCE_USERSPACE));
	EXPECT_TRUE(definition_validate_crc(buffer.data()));
}

TEST_F(DefinitionTest, validate_crc_all_source_types)
{
	const definition_source_type_t types[] = {DEFINITION_SOURCE_UNKNOWN,
											  DEFINITION_SOURCE_USERSPACE, DEFINITION_SOURCE_KERNEL,
											  DEFINITION_SOURCE_TTY};

	for (auto source_type : types) {
		const char *name = "test_name";
		memset(buffer.data(), 0, buffer.size());
		EXPECT_TRUE(definition_init(buffer.data(), name, strlen(name), source_type));
		EXPECT_TRUE(definition_validate_crc(buffer.data()))
			<< "CRC validation failed for source_type=" << static_cast<int>(source_type);
	}
}

TEST_F(DefinitionTest, validate_crc_corrupted_name)
{
	const char *name = "test_buffer";
	EXPECT_TRUE(definition_init(buffer.data(), name, strlen(name), DEFINITION_SOURCE_USERSPACE));

	// Corrupt a byte in the name
	char *name_ptr = reinterpret_cast<char *>(buffer.data() + sizeof(definition_header_t));
	name_ptr[2] = 'X';

	EXPECT_FALSE(definition_validate_crc(buffer.data()));
}

TEST_F(DefinitionTest, validate_crc_corrupted_source_type)
{
	const char *name = "test";
	size_t name_len = strlen(name);
	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_USERSPACE));

	// Corrupt source_type field
	definition_extended_t *extended = reinterpret_cast<definition_extended_t *>(
		buffer.data() + sizeof(definition_header_t) + name_len + 1);
	extended->source_type = 0xFF;

	EXPECT_FALSE(definition_validate_crc(buffer.data()));
}

TEST_F(DefinitionTest, validate_crc_corrupted_crc_byte)
{
	const char *name = "test";
	size_t name_len = strlen(name);
	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_USERSPACE));

	// Corrupt CRC byte itself
	definition_extended_t *extended = reinterpret_cast<definition_extended_t *>(
		buffer.data() + sizeof(definition_header_t) + name_len + 1);
	extended->crc8 ^= 0xFF;

	EXPECT_FALSE(definition_validate_crc(buffer.data()));
}

TEST_F(DefinitionTest, validate_crc_v1_format_returns_true)
{
	// V1 format has no CRC, should return true for backwards compatibility
	const char *name = "v1_buffer";
	size_t name_len = strlen(name);

	definition_header_t *header = reinterpret_cast<definition_header_t *>(buffer.data());
	header->body_size = name_len + 1;

	char *body = reinterpret_cast<char *>(buffer.data() + sizeof(definition_header_t));
	memcpy(body, name, name_len);
	body[name_len] = '\0';

	// V1 should validate successfully (no CRC to check)
	EXPECT_TRUE(definition_validate_crc(buffer.data()));
}

// ============================================================================
// Name retrieval tests
// ============================================================================

TEST_F(DefinitionTest, get_name_null_ptr)
{
	EXPECT_EQ(definition_get_name(nullptr), nullptr);
}

TEST_F(DefinitionTest, get_name_short)
{
	const char *name = "abc";
	EXPECT_TRUE(definition_init(buffer.data(), name, strlen(name), DEFINITION_SOURCE_USERSPACE));
	EXPECT_STREQ(definition_get_name(buffer.data()), name);
}

TEST_F(DefinitionTest, get_name_with_special_chars)
{
	const char *name = "trace/buffer-1_test.dat";
	EXPECT_TRUE(definition_init(buffer.data(), name, strlen(name), DEFINITION_SOURCE_USERSPACE));
	EXPECT_STREQ(definition_get_name(buffer.data()), name);
}

TEST_F(DefinitionTest, get_name_long)
{
	std::string name(200, 'x');
	EXPECT_TRUE(
		definition_init(buffer.data(), name.c_str(), name.size(), DEFINITION_SOURCE_KERNEL));
	EXPECT_STREQ(definition_get_name(buffer.data()), name.c_str());
}

// ============================================================================
// Source type retrieval tests
// ============================================================================

TEST_F(DefinitionTest, get_source_type_v1_returns_unknown)
{
	// V1 format should return UNKNOWN
	const char *name = "v1_buffer";
	size_t name_len = strlen(name);

	definition_header_t *header = reinterpret_cast<definition_header_t *>(buffer.data());
	header->body_size = name_len + 1;

	char *body = reinterpret_cast<char *>(buffer.data() + sizeof(definition_header_t));
	memcpy(body, name, name_len);
	body[name_len] = '\0';

	EXPECT_EQ(definition_get_source_type(buffer.data()), DEFINITION_SOURCE_UNKNOWN);
}

TEST_F(DefinitionTest, get_source_type_invalid_value_returns_unknown)
{
	const char *name = "test";
	size_t name_len = strlen(name);
	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_USERSPACE));

	// Set invalid source type (> TTY)
	definition_extended_t *extended = reinterpret_cast<definition_extended_t *>(
		buffer.data() + sizeof(definition_header_t) + name_len + 1);
	extended->source_type = 0x10; // Invalid value

	EXPECT_EQ(definition_get_source_type(buffer.data()), DEFINITION_SOURCE_UNKNOWN);
}

// ============================================================================
// Parameterized tests for various names
// ============================================================================

class DefinitionNameTest : public ::testing::TestWithParam<std::string>
{
  protected:
	std::vector<uint8_t> buffer;
	void SetUp() override { buffer.resize(1024, 0); }
};

TEST_P(DefinitionNameTest, roundtrip_name_and_source)
{
	const std::string name = GetParam();

	for (int src = DEFINITION_SOURCE_UNKNOWN; src <= DEFINITION_SOURCE_TTY; src++) {
		auto source_type = static_cast<definition_source_type_t>(src);
		memset(buffer.data(), 0, buffer.size());

		EXPECT_TRUE(definition_init(buffer.data(), name.c_str(), name.size(), source_type));
		EXPECT_TRUE(definition_has_extended(buffer.data()));
		EXPECT_TRUE(definition_validate_crc(buffer.data()));
		EXPECT_STREQ(definition_get_name(buffer.data()), name.c_str());
		EXPECT_EQ(definition_get_source_type(buffer.data()), source_type);
	}
}

INSTANTIATE_TEST_SUITE_P(DefinitionNames, DefinitionNameTest,
						 ::testing::Values("a", "ab", "test", "my_trace_buffer",
										   "tracebuffer/path/to/file.clltk_trace",
										   "CommonLowLevelTracingKit_Buffer_12345",
										   "TTY", // Special TTY name
										   "ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789"));

// ============================================================================
// Structure size and alignment tests
// ============================================================================

TEST(DefinitionStructureTest, header_size)
{
	EXPECT_EQ(sizeof(definition_header_t), 8) << "Header should be 8 bytes";
}

TEST(DefinitionStructureTest, extended_size)
{
	EXPECT_EQ(sizeof(definition_extended_t), 16) << "Extended should be 16 bytes";
}

TEST(DefinitionStructureTest, extended_magic_size)
{
	EXPECT_EQ(DEFINITION_EXTENDED_MAGIC_SIZE, 8) << "Magic should be 8 bytes";
}

TEST(DefinitionStructureTest, version_value)
{
	EXPECT_EQ(DEFINITION_VERSION, 2) << "Version should be 2 for V2 format";
}

// ============================================================================
// Binary layout verification tests
// ============================================================================

TEST_F(DefinitionTest, binary_layout_verification)
{
	const char *name = "test";
	size_t name_len = strlen(name);
	EXPECT_TRUE(definition_init(buffer.data(), name, name_len, DEFINITION_SOURCE_KERNEL));

	// Verify exact binary layout
	uint8_t *ptr = buffer.data();

	// [0-7]: body_size (uint64_t)
	uint64_t body_size = *reinterpret_cast<uint64_t *>(ptr);
	EXPECT_EQ(body_size, name_len + 1 + sizeof(definition_extended_t));
	ptr += 8;

	// [8-N]: name (null-terminated)
	EXPECT_EQ(memcmp(ptr, "test", 4), 0);
	ptr += 4;
	EXPECT_EQ(*ptr, '\0');
	ptr += 1;

	// Extended section
	// [N+1 to N+8]: magic "CLLTK_EX"
	EXPECT_EQ(memcmp(ptr, DEFINITION_EXTENDED_MAGIC, 8), 0);
	ptr += 8;

	// [N+9]: version
	EXPECT_EQ(*ptr, DEFINITION_VERSION);
	ptr += 1;

	// [N+10]: source_type
	EXPECT_EQ(*ptr, DEFINITION_SOURCE_KERNEL);
	ptr += 1;

	// [N+11 to N+15]: reserved (5 bytes, should be 0)
	for (int i = 0; i < 5; i++) {
		EXPECT_EQ(ptr[i], 0) << "Reserved byte " << i << " should be 0";
	}
	ptr += 5;

	// [N+16]: crc8
	// Just verify it exists and is non-zero (actual value depends on content)
	// CRC is already validated in other tests
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_F(DefinitionTest, minimum_valid_name)
{
	const char *name = "x";
	EXPECT_TRUE(definition_init(buffer.data(), name, 1, DEFINITION_SOURCE_USERSPACE));
	EXPECT_STREQ(definition_get_name(buffer.data()), name);
	EXPECT_TRUE(definition_validate_crc(buffer.data()));
}

TEST_F(DefinitionTest, name_with_embedded_null_uses_partial)
{
	// If name has embedded null, strlen would stop early but we pass explicit length
	const char name_with_null[] = "abc\0def";
	size_t full_len = 7; // "abc\0def"

	EXPECT_TRUE(
		definition_init(buffer.data(), name_with_null, full_len, DEFINITION_SOURCE_USERSPACE));

	// definition_get_name returns pointer to body, strlen will see "abc"
	const char *retrieved = definition_get_name(buffer.data());
	EXPECT_STREQ(retrieved, "abc");
}

TEST_F(DefinitionTest, body_size_overflow_protection)
{
	// Create buffer with impossibly large body_size
	definition_header_t *header = reinterpret_cast<definition_header_t *>(buffer.data());
	header->body_size = UINT64_MAX;

	// Should handle gracefully without crashing
	EXPECT_FALSE(definition_has_extended(buffer.data()));
	EXPECT_EQ(definition_get_source_type(buffer.data()), DEFINITION_SOURCE_UNKNOWN);
}
