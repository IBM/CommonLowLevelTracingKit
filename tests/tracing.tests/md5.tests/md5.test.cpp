// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "md5/md5.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <ostream>
#include <string.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

using namespace std::string_literals;

using type = std::tuple<std::vector<uint8_t>, std::array<char, 16>>;

class with_parameter : public ::testing::TestWithParam<type>
{
};

TEST_P(with_parameter, md5)
{
	const auto &[data, hash] = GetParam();
	MD5Context ctx = {};
	md5Init(&ctx);
	md5Update(&ctx, data.data(), data.size());
	md5Finalize(&ctx);
	if ((memcmp(ctx.digest, hash.data(), 16) == 0))
		return;
	std::stringstream hash_string;
	for (const uint8_t c : ctx.digest)
		hash_string << "\\x" << std::hex << std::setw(2) << std::setfill('0')
					<< (static_cast<unsigned int>(c) & 0xff);

	ADD_FAILURE() << "hash shoud be: " << hash_string.str();
}

type make(std::string data, const char *const hash)
{
	std::array<char, 16> _hash{};
	for (size_t i = 0; i < 16; i++) {
		_hash[i] = hash[i];
	}
	std::vector<uint8_t> _data;
	std::copy(data.begin(), data.end(), std::back_inserter(_data));
	return {_data, _hash};
}

INSTANTIATE_TEST_CASE_P(
	md5, with_parameter,
	::testing::Values(
		make("Hello world!"s, "\x86\xfb\x26\x9d\x19\x0d\x2c\x85\xf6\xe0\x46\x8c\xec\xa4\x2a\x20"),
		make("Some String"s, "\x83\xbe\xb8\xc4\xfa\x45\x96\xc8\xf7\xb5\x65\xd3\x90\xf4\x94\xe2"),
		make("A"s, "\x7f\xc5\x62\x70\xe7\xa7\x0f\xa8\x1a\x59\x35\xb7\x2e\xac\xbe\x29"),
		make("AB"s, "\xb8\x6f\xc6\xb0\x51\xf6\x3d\x73\xde\x26\x2d\x4c\x34\xe3\xa0\xa9"),
		make("ABC"s, "\x90\x2f\xbd\xd2\xb1\xdf\x0c\x4f\x70\xb4\xa5\xd2\x35\x25\xe9\x32"),
		make("ABCD"s, "\xcb\x08\xca\x4a\x7b\xb5\xf9\x68\x3c\x19\x13\x3a\x84\x87\x2c\xa7"),
		make("ABCDE"s, "\x2e\xcd\xde\x39\x59\x05\x1d\x91\x3f\x61\xb1\x45\x79\xea\x13\x6d"),
		make("ABCDEF"s, "\x88\x27\xa4\x11\x22\xa5\x02\x8b\x98\x08\xc7\xbf\x84\xb9\xfc\xf6"),
		make("ABCDEFG"s, "\xbb\x74\x7b\x3d\xf3\x13\x0f\xe1\xca\x4a\xfa\x93\xfb\x7d\x97\xc9"),
		make("ABCDEFGH"s, "\x47\x83\xe7\x84\xb4\xfa\x2f\xba\x9e\x4d\x65\x02\xdb\xc6\x4f\x8f"),
		make("ABCDEFGHI"s, "\x6f\xeb\x8a\xc0\x1a\x44\x00\xa7\x28\xb4\x82\xd0\x50\x6c\x4b\xeb"),
		make("ABCDEFGHIJ"s, "\xe8\x64\x10\xfa\x2d\x6e\x26\x34\xfd\x8a\xc5\xf4\xb3\xaf\xe7\xf3"),
		make("ABCDEFGHIJK"s, "\x30\xa4\xe3\x82\x30\x88\x5e\x27\xd1\xbb\x3f\xd0\x71\x3d\xfa\x7d"),
		make("ABCDEFGHIJKL"s, "\x2b\x0d\xc5\x68\xe5\x88\xe4\x6a\x5a\x7c\xd5\x6d\xce\xc3\x48\xaf"),
		make("ABCDEFGHIJKLM"s, "\x77\xae\xe1\x5b\xe6\x8b\xaf\xfa\x42\x8a\xea\x2f\x99\xcf\x0f\xb7"),
		make("ABCDEFGHIJKLMN"s, "\x47\xe4\x41\xc9\xbb\xd5\x71\xf9\x7f\xc8\x6c\x5b\xe3\x2f\x6c\xc0"),
		make("ABCDEFGHIJKLMNO"s,
			 "\x74\x25\x2d\x35\xb1\x7a\xf6\xe1\x5d\xd6\xe2\xde\xd3\x8b\x69\x0b"),
		make("ABCDEFGHIJKLMNOP"s,
			 "\x19\xfc\x8e\xff\x82\x03\x7f\x1f\xc0\xd8\xea\x1d\x32\xb5\xe3\x39"),
		make("ABCDEFGHIJKLMNOPQ"s,
			 "\x33\x41\x7c\x57\x9c\xd6\x47\x2a\x72\x4d\x08\x26\x02\xa8\x1d\x0e"),
		make("ABCDEFGHIJKLMNOPQR"s,
			 "\x25\x22\xae\xee\x45\x20\xae\xcc\x03\x85\x56\xc6\xe8\xb6\xa6\x0b"),
		make("ABCDEFGHIJKLMNOPQRS"s,
			 "\x2b\x6b\x1a\x3e\x87\x9a\xdb\xf5\x52\xb2\x38\xba\x74\x46\x37\x6c"),
		make("ABCDEFGHIJKLMNOPQRST"s,
			 "\x62\x78\x73\x63\x82\xb5\x77\x31\xaf\xfc\x96\xab\x19\x07\xb7\x9f"),
		make("ABCDEFGHIJKLMNOPQRSTU"s,
			 "\x4f\x09\xc0\x65\xa9\xa2\x38\xa5\xc9\xc5\xdf\xb5\xb7\x7d\xb8\x0b"),
		make("ABCDEFGHIJKLMNOPQRSTUV"s,
			 "\xd6\xbf\xe5\xcf\xa3\xaf\xe4\xda\x68\x63\x9d\xd9\x0a\x0f\xe3\xbe"),
		make("ABCDEFGHIJKLMNOPQRSTUVW"s,
			 "\x51\x81\x42\xf4\xe5\x7f\x65\xa4\x72\x29\x71\xd6\x30\xc0\x89\xa0"),
		make("ABCDEFGHIJKLMNOPQRSTUVWX"s,
			 "\xea\xef\xdf\xcf\xf8\x4f\xfe\xe3\x05\x0c\x64\x42\xfd\xfb\x04\x33"),
		make("ABCDEFGHIJKLMNOPQRSTUVWXY"s,
			 "\x5d\xfc\xeb\x53\x37\x1f\x1e\x96\x0f\xec\x41\x4b\xaf\x31\x5e\xe1"),
		make("ABCDEFGHIJKLMNOPQRSTUVWXYZ"s,
			 "\x43\x7b\xba\x8e\x0b\xf5\x83\x37\x67\x4f\x45\x39\xe7\x51\x86\xac"),
		make("01 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "02 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "03 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "04 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "05 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "06 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "07 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "08 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "09 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "10 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s,
			 "\xe2\xad\xd5\x10\x0e\xb4\x7a\x75\xd3\x58\x93\x99\xda\x3c\x6e\x59"),
		make("01 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "02 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "03 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "04 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "05 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "06 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "07 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "08 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "09 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "10 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "11 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "12 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "13 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "14 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "15 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "16 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "17 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "18 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s +
				 "19 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s + "20 ABCDEFGHIJKLMNOPQRSTUVWXYZ - "s,
			 "\xa7\xeb\x90\x4a\xfb\xf0\xab\x5a\x77\xae\x11\x3f\xd4\xd5\x1d\xbc")

			));