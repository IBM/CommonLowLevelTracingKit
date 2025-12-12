// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "CommonLowLevelTracingKit/decoder/Meta.hpp"
#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "helper.hpp"
#include "meta_parser.hpp"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::source;
namespace fs = std::filesystem;

// ============================================================================
// MetaParser Unit Tests
// ============================================================================

class MetaParserTest : public ::testing::Test
{
  protected:
	std::vector<uint8_t> buildMetaEntry(uint8_t type, uint32_t line, const std::string &arg_types,
										const std::string &file, const std::string &format)
	{
		std::vector<uint8_t> entry;
		uint32_t size =
			static_cast<uint32_t>(11 + arg_types.size() + 1 + file.size() + 1 + format.size() + 1);

		entry.push_back('{');
		entry.resize(entry.size() + 4);
		std::memcpy(entry.data() + 1, &size, sizeof(size));
		entry.push_back(type);
		entry.resize(entry.size() + 4);
		std::memcpy(entry.data() + 6, &line, sizeof(line));
		entry.push_back(static_cast<uint8_t>(arg_types.size()));

		for (char c : arg_types) { entry.push_back(static_cast<uint8_t>(c)); }
		entry.push_back(0);

		for (char c : file) { entry.push_back(static_cast<uint8_t>(c)); }
		entry.push_back(0);

		for (char c : format) { entry.push_back(static_cast<uint8_t>(c)); }
		entry.push_back(0);

		return entry;
	}
};

TEST_F(MetaParserTest, parse_empty_data)
{
	std::vector<uint8_t> empty;
	auto result = MetaParser::parse(std::span<const uint8_t>(empty), 0);
	EXPECT_TRUE(result.empty());
}

TEST_F(MetaParserTest, parse_single_entry)
{
	auto entry = buildMetaEntry(1, 42, "is", "test.cpp", "value=%d str=%s");
	auto result = MetaParser::parse(std::span<const uint8_t>(entry), 0);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].type, MetaEntryType::Printf);
	EXPECT_EQ(result[0].line, 42);
	EXPECT_EQ(result[0].arg_count, 2);
	EXPECT_EQ(result[0].arg_types, "is");
	EXPECT_EQ(result[0].file, "test.cpp");
	EXPECT_EQ(result[0].format, "value=%d str=%s");
}

TEST_F(MetaParserTest, parse_multiple_entries)
{
	auto entry1 = buildMetaEntry(1, 10, "i", "a.cpp", "fmt1");
	auto entry2 = buildMetaEntry(1, 20, "s", "b.cpp", "fmt2");

	std::vector<uint8_t> combined;
	combined.insert(combined.end(), entry1.begin(), entry1.end());
	combined.insert(combined.end(), entry2.begin(), entry2.end());

	auto result = MetaParser::parse(std::span<const uint8_t>(combined), 0);

	ASSERT_EQ(result.size(), 2);
	EXPECT_EQ(result[0].line, 10);
	EXPECT_EQ(result[0].file, "a.cpp");
	EXPECT_EQ(result[1].line, 20);
	EXPECT_EQ(result[1].file, "b.cpp");
}

TEST_F(MetaParserTest, parse_skips_invalid_magic)
{
	std::vector<uint8_t> data = {0x00, 0x00, 0x00};
	auto entry = buildMetaEntry(1, 100, "", "file.c", "msg");
	data.insert(data.end(), entry.begin(), entry.end());

	auto result = MetaParser::parse(std::span<const uint8_t>(data), 0);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].line, 100);
	EXPECT_EQ(result[0].offset, 3);
}

TEST_F(MetaParserTest, parse_validates_size)
{
	std::vector<uint8_t> data = {'{', 0xFF, 0xFF, 0xFF, 0xFF};
	auto result = MetaParser::parse(std::span<const uint8_t>(data), 0);
	EXPECT_TRUE(result.empty());
}

TEST_F(MetaParserTest, parse_dump_type)
{
	auto entry = buildMetaEntry(2, 50, "x", "dump.cpp", "data dump");
	auto result = MetaParser::parse(std::span<const uint8_t>(entry), 0);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].type, MetaEntryType::Dump);
}

TEST_F(MetaParserTest, parse_unknown_type)
{
	auto entry = buildMetaEntry(99, 50, "", "unknown.cpp", "unknown");
	auto result = MetaParser::parse(std::span<const uint8_t>(entry), 0);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].type, MetaEntryType::Unknown);
}

TEST_F(MetaParserTest, isValidMagic)
{
	EXPECT_TRUE(MetaParser::isValidMagic('{'));
	EXPECT_FALSE(MetaParser::isValidMagic('['));
	EXPECT_FALSE(MetaParser::isValidMagic(0));
}

TEST_F(MetaParserTest, parse_truncated_entry)
{
	auto entry = buildMetaEntry(1, 42, "is", "test.cpp", "value=%d str=%s");
	// Truncate the entry (remove last 5 bytes)
	entry.resize(entry.size() - 5);

	auto result = MetaParser::parse(std::span<const uint8_t>(entry), 0);
	EXPECT_TRUE(result.empty());
}

TEST_F(MetaParserTest, parse_zero_size_entry)
{
	std::vector<uint8_t> data = {'{', 0x00, 0x00, 0x00, 0x00};
	auto result = MetaParser::parse(std::span<const uint8_t>(data), 0);
	EXPECT_TRUE(result.empty());
}

TEST_F(MetaParserTest, parse_max_arg_count)
{
	std::string arg_types(255, 'i');
	auto entry = buildMetaEntry(1, 100, arg_types, "max_args.cpp", "many args");
	auto result = MetaParser::parse(std::span<const uint8_t>(entry), 0);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].arg_count, 255);
	EXPECT_EQ(result[0].arg_types.size(), 255);
}

TEST_F(MetaParserTest, parse_empty_strings)
{
	auto entry = buildMetaEntry(1, 1, "", "", "");
	auto result = MetaParser::parse(std::span<const uint8_t>(entry), 0);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].arg_count, 0);
	EXPECT_EQ(result[0].arg_types, "");
	EXPECT_EQ(result[0].file, "");
	EXPECT_EQ(result[0].format, "");
}

TEST_F(MetaParserTest, parse_tracks_offset_correctly)
{
	auto entry1 = buildMetaEntry(1, 10, "i", "a.cpp", "fmt1");
	auto entry2 = buildMetaEntry(1, 20, "s", "b.cpp", "fmt2");

	std::vector<uint8_t> combined;
	combined.insert(combined.end(), entry1.begin(), entry1.end());
	combined.insert(combined.end(), entry2.begin(), entry2.end());

	auto result = MetaParser::parse(std::span<const uint8_t>(combined), 100);

	ASSERT_EQ(result.size(), 2);
	EXPECT_EQ(result[0].offset, 100);
	EXPECT_EQ(result[1].offset, 100 + entry1.size());
}

// ============================================================================
// MetaParser Type Mapping Parameterized Tests
// ============================================================================

class MetaEntryTypeParamTest
	: public MetaParserTest,
	  public ::testing::WithParamInterface<std::tuple<uint8_t, MetaEntryType>>
{
};

TEST_P(MetaEntryTypeParamTest, type_mapping)
{
	const auto &[raw_type, expected_type] = GetParam();
	auto entry = buildMetaEntry(raw_type, 1, "", "f.c", "fmt");
	auto result = MetaParser::parse(std::span<const uint8_t>(entry), 0);
	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].type, expected_type);
}

INSTANTIATE_TEST_SUITE_P(TypeMappings, MetaEntryTypeParamTest,
						 ::testing::Values(std::make_tuple(0, MetaEntryType::Unknown),
										   std::make_tuple(1, MetaEntryType::Printf),
										   std::make_tuple(2, MetaEntryType::Dump),
										   std::make_tuple(99, MetaEntryType::Unknown),
										   std::make_tuple(255, MetaEntryType::Unknown)));

// ============================================================================
// MetaEntryInfo Unit Tests
// ============================================================================

class MetaEntryInfoTest : public ::testing::Test
{
};

TEST_F(MetaEntryInfoTest, typeToString)
{
	EXPECT_EQ(MetaEntryInfo::typeToString(MetaEntryType::Printf), "printf");
	EXPECT_EQ(MetaEntryInfo::typeToString(MetaEntryType::Dump), "dump");
	EXPECT_EQ(MetaEntryInfo::typeToString(MetaEntryType::Unknown), "unknown");
}

TEST_F(MetaEntryInfoTest, argCharToTypeName)
{
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('c'), "uint8");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('C'), "int8");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('w'), "uint16");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('W'), "int16");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('i'), "uint32");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('I'), "int32");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('l'), "uint64");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('L'), "int64");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('f'), "float");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('d'), "double");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('s'), "string");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('p'), "pointer");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('x'), "dump");
	EXPECT_EQ(MetaEntryInfo::argCharToTypeName('?'), "unknown");
}

TEST_F(MetaEntryInfoTest, argumentTypeNames)
{
	MetaEntryInfo info;
	info.arg_types = "isL";

	auto names = info.argumentTypeNames();

	ASSERT_EQ(names.size(), 3);
	EXPECT_EQ(names[0], "uint32");
	EXPECT_EQ(names[1], "string");
	EXPECT_EQ(names[2], "int64");
}

TEST_F(MetaEntryInfoTest, argumentTypeNames_empty)
{
	MetaEntryInfo info;
	info.arg_types = "";

	auto names = info.argumentTypeNames();
	EXPECT_TRUE(names.empty());
}

// ============================================================================
// Standalone Tests
// ============================================================================

TEST(MetaSourceTypeTest, metaSourceTypeToString)
{
	EXPECT_EQ(metaSourceTypeToString(MetaSourceType::Tracebuffer), "tracebuffer");
	EXPECT_EQ(metaSourceTypeToString(MetaSourceType::Snapshot), "snapshot");
	EXPECT_EQ(metaSourceTypeToString(MetaSourceType::ElfSection), "elf");
	EXPECT_EQ(metaSourceTypeToString(MetaSourceType::RawBlob), "raw");
}

// ============================================================================
// MetaIntegration Tests
// ============================================================================

class MetaIntegrationTest : public ::testing::Test
{
  protected:
	std::string m_tracebuffer_name;
	fs::path m_file_path;

	void SetUp() override
	{
		static int index = 0;
		m_tracebuffer_name = "_meta_integration_test_" + std::to_string(index++);
		m_file_path = trace_file(m_tracebuffer_name);
		if (fs::exists(m_file_path)) { fs::remove(m_file_path); }
	}

	void TearDown() override
	{
		if (fs::exists(m_file_path)) { fs::remove(m_file_path); }
	}
};

TEST_F(MetaIntegrationTest, getMetaInfo_nonexistent_path)
{
	auto result = getMetaInfo("/nonexistent/path/to/file");
	EXPECT_TRUE(result.empty());
}

TEST_F(MetaIntegrationTest, getMetaInfo_empty_directory)
{
	auto temp_dir = fs::temp_directory_path() / "clltk_meta_test_empty";
	fs::create_directories(temp_dir);

	auto result = getMetaInfo(temp_dir);
	EXPECT_TRUE(result.empty());

	fs::remove_all(temp_dir);
}

TEST_F(MetaIntegrationTest, hasMetaInfo_nonexistent)
{
	EXPECT_FALSE(hasMetaInfo("/nonexistent/file.clltk_trace"));
}

TEST_F(MetaIntegrationTest, getMetaInfo_from_tracebuffer)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 4096);
	clltk_dynamic_tracepoint_execution(m_tracebuffer_name.c_str(), __FILE__, __LINE__, 0, 0,
									   "test message %d", 42);

	auto result = getMetaInfo(m_file_path);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].name, m_tracebuffer_name);
	EXPECT_EQ(result[0].source_type, MetaSourceType::Tracebuffer);
	EXPECT_TRUE(result[0].valid());
	// Dynamic tracebuffers don't have static meta entries in the stack section
	// They only have runtime-generated trace points
}

TEST_F(MetaIntegrationTest, getMetaInfo_with_filter)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 4096);

	auto filter = [&](const std::string &name) { return name == m_tracebuffer_name; };
	auto result = getMetaInfo(m_file_path, false, filter);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].name, m_tracebuffer_name);
}

TEST_F(MetaIntegrationTest, getMetaInfo_filter_excludes)
{
	clltk_dynamic_tracebuffer_creation(m_tracebuffer_name.c_str(), 4096);

	auto filter = [](const std::string &name) { return name == "nonexistent"; };
	auto result = getMetaInfo(m_file_path, false, filter);

	EXPECT_TRUE(result.empty());
}

TEST_F(MetaIntegrationTest, MetaSourceInfo_valid)
{
	MetaSourceInfo info;
	EXPECT_TRUE(info.valid());

	info.error = "some error";
	EXPECT_FALSE(info.valid());
}

// ============================================================================
// MetaStaticTrace Tests
// ============================================================================

CLLTK_TRACEBUFFER(META_TEST_STATIC, 4096)

class MetaStaticTraceTest : public ::testing::Test
{
  protected:
	void TearDown() override
	{
		fs::path file_path = trace_file("META_TEST_STATIC");
		if (fs::exists(file_path)) { fs::remove(file_path); }
	}
};

TEST_F(MetaStaticTraceTest, getMetaInfo_static_tracebuffer)
{
	CLLTK_TRACEPOINT(META_TEST_STATIC, "static test %d %s", 123, "hello");

	fs::path file_path = trace_file("META_TEST_STATIC");
	auto result = getMetaInfo(file_path);

	ASSERT_EQ(result.size(), 1);
	EXPECT_EQ(result[0].name, "META_TEST_STATIC");
	EXPECT_TRUE(result[0].valid());
	EXPECT_FALSE(result[0].entries.empty());

	bool found_our_tracepoint = false;
	for (const auto &entry : result[0].entries) {
		if (entry.format.find("static test") != std::string::npos) {
			found_our_tracepoint = true;
			EXPECT_EQ(entry.type, MetaEntryType::Printf);
			EXPECT_EQ(entry.arg_count, 2);
			break;
		}
	}
	EXPECT_TRUE(found_our_tracepoint);
}
