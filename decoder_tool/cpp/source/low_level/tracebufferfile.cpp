
#include "tracebufferfile.hpp"

#include <array>
#include <stdexcept>

namespace source = CommonLowLevelTracingKit::decoder::source;
using namespace std::string_literals;

source::TracebufferFile::TracebufferFile(const std::string &p_path)
	: m_file(p_path)
	, m_definition(getFilePart(getFileHeader().get<uint64_t>(24)))
	, m_ringbuffer(getFilePart(getFileHeader().get<uint64_t>(32))) {
	if (!validFile()) { throw std::runtime_error("In valid tracebuffer " + p_path); }
	if (!getDefinition().isValid()) { throw std::runtime_error("In valid definition " + p_path); }
}

bool source::TracebufferFile::getFileHeaderMagicValid() const {
	using MagicType = std::array<const char, 16>;
	const MagicType expected = {'?', '#', '$', '~', 't', 'r', 'a', 'c',
								'e', 'b', 'u', 'f', 'f', 'e', 'r', '\0'};
	const auto magic = getFilePart().get<MagicType>();
	return expected == magic;
}
source::TracebufferFile::VersionType source::TracebufferFile::getVersion() const {
	const auto rawVersion = getFilePart().get<uint64_t>(16);
	const uint8_t patch = (0xff & (rawVersion / 0x00001));
	const uint8_t minor = (0xff & (rawVersion / 0x00100));
	const uint8_t major = (0xff & (rawVersion / 0x10000));
	return {major, minor, patch};
}