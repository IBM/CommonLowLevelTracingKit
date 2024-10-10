// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <limits.h>
#include <optional>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace std::string_literals;

class SnapshotTest : public ::testing::Test
{

  protected:
	fs::path temp_dir;
	static constexpr unsigned int seed = 0;
	void SetUp() override
	{
		std::mt19937 engine(seed);
		std::uniform_int_distribution<char> dist(CHAR_MIN, CHAR_MAX);

		const auto time_point = std::chrono::system_clock::now().time_since_epoch();
		const auto millis =
			std::chrono::duration_cast<std::chrono::milliseconds>(time_point).count();
		temp_dir = fs::temp_directory_path() / ("SnapshotTest_" + std::to_string(millis));
		EXPECT_TRUE(fs::create_directory(temp_dir));
		EXPECT_EQ(0, setenv("CLLTK_TRACING_PATH", temp_dir.c_str(), 0));
		for (int i = 0; i < 32; i++) {
			std::string file_name =
				"newFile"s + std::to_string(i) + " _" + std::to_string(dist(engine)) + ".bin"s;
			fs::path filepath = this->temp_dir / file_name;
			std::ofstream file(filepath, std::ios::binary);
			std::vector<char> buffer(1024);
			std::generate(buffer.begin(), buffer.end(), [&]() { return dist(engine); });
			file.write(buffer.data(), (long)buffer.size());
		}
	}
	void TearDown() override
	{
		EXPECT_EQ(0, unsetenv("CLLTK_TRACING_PATH"));
		if (fs::exists(temp_dir)) {
			fs::remove_all(temp_dir);
		}
	}
};

static size_t countOpenFileDescriptors(void)
{

	const pid_t pid = getpid();
	const std::string path = "/proc/" + std::to_string(pid) + "/fd";
	DIR *const dir = opendir(path.c_str());
	if (dir == nullptr) {
		EXPECT_TRUE(false) << "Failed to open directory: " << path << std::endl;
		return 0; // Return -1 on error
	}

	struct dirent *entry;
	size_t count = 0;
	while ((entry = readdir(dir)) != nullptr) {
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
			++count; // Increment count for each entry except "." and ".."
		}
	}
	closedir(dir);
	return count;
}

using namespace CommonLowLevelTracingKit::snapshot;

TEST_F(SnapshotTest, TestInfrastructur)
{
	const size_t oldOpenFdCounter = countOpenFileDescriptors();
	EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
	std::FILE *const tmpf = std::tmpfile();
	EXPECT_LT(oldOpenFdCounter, countOpenFileDescriptors());
	std::fclose(tmpf);
	EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
}

TEST_F(SnapshotTest, TestUncompressedSnapshot)
{
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto count = take_snapshot(func, {}, false);
	ASSERT_TRUE(count);
	EXPECT_TRUE(count.value());
}

TEST_F(SnapshotTest, TestCompressedSnapshot)
{
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto count = take_snapshot(func, {}, true);
	ASSERT_TRUE(count);
	EXPECT_TRUE(count.value());
}

TEST_F(SnapshotTest, AllFileClosedInUnCompressedSnapshot)
{
	const size_t oldOpenFdCounter = countOpenFileDescriptors();
	static const write_function_t func = [=](const void *, size_t size) -> std::optional<size_t> {
		static bool first = true;
		if (first) {
			first = false;
			EXPECT_LT(oldOpenFdCounter, countOpenFileDescriptors());
		}
		return size;
	};
	const auto count = take_snapshot(func, {}, false);
	ASSERT_TRUE(count);
	EXPECT_TRUE(count.value());
	EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
}

TEST_F(SnapshotTest, AllFileClosedInCompressedSnapshot)
{
	const size_t oldOpenFdCounter = countOpenFileDescriptors();
	static const write_function_t func = [=](const void *, size_t size) -> std::optional<size_t> {
		static bool first = true;
		if (first) {
			first = false;
			EXPECT_LE(oldOpenFdCounter, countOpenFileDescriptors());
		}
		return size;
	};
	const auto count = take_snapshot(func, {}, true);
	ASSERT_TRUE(count);
	EXPECT_TRUE(count.value());
	EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
}

TEST_F(SnapshotTest, TwiceUncomporessedSameSize)
{
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto firstSize = take_snapshot(func, {}, false);
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());
	const auto secondSize = take_snapshot(func, {}, false);
	EXPECT_TRUE(secondSize);
	EXPECT_TRUE(secondSize.value());
	EXPECT_EQ(firstSize.value(), secondSize.value());
}

TEST_F(SnapshotTest, CompressedIsSmallerThanRaw)
{
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto firstSize = take_snapshot(func, {}, true);
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());

	{
		std::mt19937 engine(seed);
		std::uniform_int_distribution<char> dist(CHAR_MIN, CHAR_MAX);
		fs::path filepath = this->temp_dir / "newFile.bin";
		std::ofstream file(filepath, std::ios::binary);
		std::vector<char> buffer(1024 * 1024);
		std::generate(buffer.begin(), buffer.end(), [&]() { return dist(engine); });
		file.write(buffer.data(), (long)buffer.size());
	}

	const auto secondSize = take_snapshot(func, {}, true);
	EXPECT_TRUE(secondSize);
	EXPECT_TRUE(secondSize.value());
	EXPECT_LT(secondSize.value(), firstSize.value() + 1024 * 1024);
}

TEST_F(SnapshotTest, UncompressedTwiceWithAdditionalTracepointDifferSize)
{
	const write_function_t func = [&](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto firstSize = take_snapshot(func, {});
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());
	const static std::vector<std::string> tracepoints{1024, {"some more tracepoints"}};
	const auto secondSize = take_snapshot(func, tracepoints);
	EXPECT_TRUE(secondSize);
	EXPECT_TRUE(secondSize.value());
	EXPECT_LT(firstSize.value(), secondSize.value());
}

TEST_F(SnapshotTest, CompressedTwiceWithAdditionalTracepointDifferSize)
{
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto firstSize = take_snapshot(func, {}, true);
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());
	const static std::vector<std::string> tracepoints{1024, {"some more tracepoints"}};
	const auto secondSize = take_snapshot(func, tracepoints, true);
	EXPECT_TRUE(secondSize);
	EXPECT_TRUE(secondSize.value());
	EXPECT_LT(firstSize.value(), secondSize.value());
}

TEST_F(SnapshotTest, TwiceComporessedSameSize)
{
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto firstSize = take_snapshot(func, {}, true);
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());
	const auto secondSize = take_snapshot(func, {}, true);
	EXPECT_TRUE(secondSize);
	EXPECT_TRUE(secondSize.value());
	EXPECT_EQ(firstSize.value(), secondSize.value());
}

TEST_F(SnapshotTest, ComporessedIsLessThanUncompressed)
{
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size;
	};
	const auto firstSize = take_snapshot(func, {}, true);
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());
	const auto secondSize = take_snapshot(func, {}, false);
	EXPECT_TRUE(secondSize);
	EXPECT_TRUE(secondSize.value());
	EXPECT_LT(firstSize.value(), secondSize.value());
}

TEST_F(SnapshotTest, UncompressedIsTar)
{
	constexpr char TAR_MAGIC[] = {"ustar"};
	constexpr size_t TAR_MAGIC_OFFSET = 257;
	size_t count = 0;
	write_function_t func = [&](const void *data, size_t size) -> std::optional<size_t> {
		const char *const c = (const char *)data;
		if (count <= TAR_MAGIC_OFFSET and TAR_MAGIC_OFFSET <= count + size) {
			const char *const name = &c[TAR_MAGIC_OFFSET - count];
			EXPECT_TRUE(memcmp(name, TAR_MAGIC, sizeof(TAR_MAGIC) - 1) == 0) << c;
		}
		count += size;
		return size;
	};
	const auto firstSize = take_snapshot(func, {}, false);
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());
}

TEST_F(SnapshotTest, UncompressedIsGzip)
{
	constexpr char TAR_MAGIC[] = {"\x1f\x8b"};
	size_t count = 0;
	write_function_t func = [&](const void *data, size_t size) -> std::optional<size_t> {
		if (count == 0) {
			EXPECT_TRUE(memcmp(data, TAR_MAGIC, sizeof(TAR_MAGIC) - 1) == 0);
		}
		count += size;
		return size;
	};
	const auto firstSize = take_snapshot(func, {}, true);
	EXPECT_TRUE(firstSize);
	EXPECT_TRUE(firstSize.value());
}

TEST_F(SnapshotTest, WriteFailedTotal)
{
	const size_t oldOpenFdCounter = countOpenFileDescriptors();
	static const write_function_t func = [](const void *, size_t) -> std::optional<size_t> {
		return {};
	};
	{
		const auto count = take_snapshot(func, {}, false);
		ASSERT_FALSE(count);
		EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
	}
	{
		const auto count = take_snapshot(func, {}, true);
		ASSERT_FALSE(count);
		EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
	}
}

TEST_F(SnapshotTest, WriteFailedALittle)
{
	const size_t oldOpenFdCounter = countOpenFileDescriptors();
	static const write_function_t func = [](const void *, size_t size) -> std::optional<size_t> {
		return size - 1;
	};
	{
		const auto count = take_snapshot(func, {}, true);
		ASSERT_FALSE(count);
		EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
	}
	{
		const auto count = take_snapshot(func, {}, true);
		ASSERT_FALSE(count);
		EXPECT_EQ(oldOpenFdCounter, countOpenFileDescriptors());
	}
}

static int mmapCountNow()
{
	std::ifstream mapsFile("/proc/self/maps");
	std::string line;
	int entries = 0;
	while (std::getline(mapsFile, line)) {
		entries++;
	}
	return entries;
}
TEST_F(SnapshotTest, CheckForMissingMUNMAP)
{
	const int mmapCountBefore = mmapCountNow();

	const write_function_t func = [&](const void *, size_t size) -> std::optional<size_t> {
		EXPECT_GE(mmapCountNow(), mmapCountBefore);
		return size;
	};
	const auto count = take_snapshot(func, {}, false);
	ASSERT_TRUE(count);
	EXPECT_TRUE(count.value());
	const int mmapCountAfter = mmapCountNow();
	EXPECT_EQ(mmapCountBefore, mmapCountAfter);
}
