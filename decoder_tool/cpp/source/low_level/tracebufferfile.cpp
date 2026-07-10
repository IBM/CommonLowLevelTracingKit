
#include "tracebufferfile.hpp"

#include <array>
#include <bit>
#include <stdexcept>

namespace source = CommonLowLevelTracingKit::decoder::source;
using namespace std::string_literals;

using MagicType = std::array<char, 16>;
// the writer stores the magic as two native-endian uint64 words, so the byte
// pattern in the file identifies the writer's byte order
static constexpr MagicType little_endian_magic = {'?', '#', '$', '~', 't', 'r', 'a', 'c',
												  'e', 'b', 'u', 'f', 'f', 'e', 'r', '\0'};
static constexpr MagicType big_endian_magic = {'c',	 'a', 'r', 't', '~', '$', '#', '?',
											   '\0', 'r', 'e', 'f', 'f', 'u', 'b', 'e'};

// open the file and mark it foreign-endian when the magic byte pattern shows
// it was written on a machine with the opposite byte order; all multi-byte
// reads through the FilePart then byte-swap transparently
static source::FilePart makeFilePart(const std::string &path) {
	source::FilePart file{path};
	const auto magic = file.get<MagicType>(0);
	if constexpr (std::endian::native == std::endian::little) {
		file.setForeignEndian(magic == big_endian_magic);
	} else {
		file.setForeignEndian(magic == little_endian_magic);
	}
	return file;
}

source::TracebufferFile::TracebufferFile(const std::string &p_path)
	: m_file(makeFilePart(p_path))
	, m_definition(getFilePart(getFileHeader().get<uint64_t>(24)))
	, m_ringbuffer(getFilePart(getFileHeader().get<uint64_t>(32))) {
	if (!validFile()) { throw std::runtime_error("In valid tracebuffer " + p_path); }
	if (!getDefinition().isValid()) { throw std::runtime_error("In valid definition " + p_path); }
}

bool source::TracebufferFile::getFileHeaderMagicValid() const {
	const auto magic = getFilePart().get<MagicType>();
	return (magic == little_endian_magic) || (magic == big_endian_magic);
}
source::TracebufferFile::VersionType source::TracebufferFile::getVersion() const {
	const auto rawVersion = getFilePart().get<uint64_t>(16);
	const uint8_t patch = (0xff & (rawVersion / 0x00001));
	const uint8_t minor = (0xff & (rawVersion / 0x00100));
	const uint8_t major = (0xff & (rawVersion / 0x10000));
	return {major, minor, patch};
}