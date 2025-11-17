#ifndef DECODER_TOOL_SOURCE_TRACEBUFFER_HEADER
#define DECODER_TOOL_SOURCE_TRACEBUFFER_HEADER
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <tuple>

#include "CommonLowLevelTracingKit/Decoder/Tracepoint.hpp"
#include "definition.hpp"
#include "file.hpp"
#include "inline.hpp"
#include "ringbuffer.hpp"

namespace CommonLowLevelTracingKit::decoder::source {
	class TracebufferFile final {
	  public:
		TracebufferFile(const std::string &path);
		TracebufferFile(const TracebufferFile &) = delete;
		TracebufferFile &operator=(const TracebufferFile &) = delete;
		TracebufferFile(TracebufferFile &&) = delete;
		TracebufferFile &operator=(TracebufferFile &&) = delete;
		CONST_INLINE const FilePart &getFilePart() const { return m_file; }
		INLINE FilePart getFilePart(size_t offset) const { return m_file.get<FilePart>(offset); }

		using VersionType = const std::tuple<const uint8_t, const uint8_t, const uint8_t>;
		VersionType getVersion() const;
		INLINE const Definition &getDefinition() const { return m_definition; }
		INLINE Ringbuffer &getRingbuffer() { return m_ringbuffer; }
		INLINE const Ringbuffer &getRingbuffer() const { return m_ringbuffer; }

	  protected:
		CONST_INLINE bool validFile() const {
			return getFileHeaderMagicValid() && getFileHeaderCrcValid() &&
				   (m_ringbuffer.getSize() > 10);
		}
		CONST_INLINE FilePart getFileHeader() const { return m_file.get(0); }
		CONST_INLINE bool getFileHeaderCrcValid() const { return getFileHeader().crc8(56) == 0; }
		bool getFileHeaderMagicValid() const;

	  private:
		const FilePart m_file;
		Definition m_definition;
		Ringbuffer m_ringbuffer;
	};
} // namespace CommonLowLevelTracingKit::decoder::source

#endif