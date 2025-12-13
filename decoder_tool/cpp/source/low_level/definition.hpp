#ifndef DECODER_TOOL_SOURCE_DEFINITION_HEADER
#define DECODER_TOOL_SOURCE_DEFINITION_HEADER
#include "CommonLowLevelTracingKit/decoder/Inline.hpp"
#include "file.hpp"
#include <cstring>
#include <string_view>

namespace CommonLowLevelTracingKit::decoder::source {

	/**
	 * @brief Source type for trace origin identification (internal)
	 *
	 * Encoding uses 2 bits:
	 *   00 = Unknown (legacy files or unspecified)
	 *   01 = Userspace
	 *   10 = Kernel
	 *   11 = TTY (kernel trace where buffer name is "TTY")
	 */
	enum class DefinitionSourceType : uint8_t {
		Unknown = 0x00,
		Userspace = 0x01,
		Kernel = 0x02,
		TTY = 0x03,
	};

	/**
	 * @brief Extended definition header magic
	 */
	static constexpr std::string_view DEFINITION_EXTENDED_MAGIC{"CLLTK_EX"};
	static constexpr size_t DEFINITION_EXTENDED_MAGIC_SIZE = 8;

	/**
	 * @brief Extended definition fields (V2 format)
	 *
	 * Layout after null-terminated name:
	 *   [0-7]   magic "CLLTK_EX"
	 *   [8]     version
	 *   [9]     source_type
	 *   [10-14] reserved
	 *   [15]    crc8
	 */
	struct DefinitionExtended {
		char magic[DEFINITION_EXTENDED_MAGIC_SIZE];
		uint8_t version;
		uint8_t source_type;
		uint8_t reserved[5];
		uint8_t crc8;
	};
	static_assert(sizeof(DefinitionExtended) == 16, "DefinitionExtended size mismatch");

	class Definition final {
	  public:
		Definition(const FilePart &&a_file)
			: m_file(a_file)
			, m_body_size(a_file.get<uint64_t>(0))
			, m_name(parseNameV2(a_file))
			, m_source_type(parseSourceType(a_file))
			, m_has_extended(checkHasExtended(a_file))
			, m_crc_valid(validateCrc(a_file)) {};

		Definition(const Definition &) = delete;
		Definition &operator=(const Definition &) = delete;
		Definition(Definition &&) = default;
		Definition &operator=(Definition &&) = delete;

		CONST_INLINE const std::string_view name() const { return m_name; }

		CONST_INLINE bool isValid() const {
			return !m_name.empty() && m_name.size() < m_file.getFileSize() &&
				   (!m_has_extended || m_crc_valid);
		}

		CONST_INLINE DefinitionSourceType sourceType() const { return m_source_type; }

		CONST_INLINE bool hasExtended() const { return m_has_extended; }

		CONST_INLINE bool isCrcValid() const { return m_crc_valid; }

	  private:
		const FilePart m_file;
		const uint64_t m_body_size;
		const std::string_view m_name;
		const DefinitionSourceType m_source_type;
		const bool m_has_extended;
		const bool m_crc_valid;

		/**
		 * @brief Parse name from V2 format (null-terminated in body)
		 */
		static std::string_view parseNameV2(const FilePart &file) {
			const uint64_t body_size = file.get<uint64_t>(0);
			if (body_size == 0 || body_size > file.getFileSize()) { return {}; }

			// Name starts at offset 8 (after body_size field)
			const char *body = &file.getReference<char>(sizeof(uint64_t));

			// Find null terminator within body
			size_t name_length = 0;
			while (name_length < body_size && body[name_length] != '\0') { name_length++; }

			return std::string_view(body, name_length);
		}

		/**
		 * @brief Check if definition has extended format (V2)
		 */
		static bool checkHasExtended(const FilePart &file) {
			const uint64_t body_size = file.get<uint64_t>(0);
			if (body_size == 0 || body_size > file.getFileSize()) { return false; }

			const char *body = &file.getReference<char>(sizeof(uint64_t));

			// Find end of name
			size_t name_length = 0;
			while (name_length < body_size && body[name_length] != '\0') { name_length++; }

			// Check if there's enough space after name for extended fields
			const size_t remaining = body_size - (name_length + 1);
			if (remaining < sizeof(DefinitionExtended)) { return false; }

			// Check for extended magic
			const char *extended_ptr = body + name_length + 1;
			return std::memcmp(extended_ptr, DEFINITION_EXTENDED_MAGIC.data(),
							   DEFINITION_EXTENDED_MAGIC_SIZE) == 0;
		}

		/**
		 * @brief Parse source type from definition
		 */
		static DefinitionSourceType parseSourceType(const FilePart &file) {
			if (!checkHasExtended(file)) { return DefinitionSourceType::Unknown; }

			const uint64_t body_size = file.get<uint64_t>(0);
			const char *body = &file.getReference<char>(sizeof(uint64_t));

			// Find end of name
			size_t name_length = 0;
			while (name_length < body_size && body[name_length] != '\0') { name_length++; }

			const DefinitionExtended *extended =
				reinterpret_cast<const DefinitionExtended *>(body + name_length + 1);

			uint8_t source = extended->source_type;
			if (source > static_cast<uint8_t>(DefinitionSourceType::TTY)) {
				return DefinitionSourceType::Unknown;
			}

			return static_cast<DefinitionSourceType>(source);
		}

		/**
		 * @brief Validate CRC for V2 format
		 */
		static bool validateCrc(const FilePart &file) {
			if (!checkHasExtended(file)) {
				// V1 format has no CRC, consider valid for backwards compatibility
				return true;
			}

			const uint64_t body_size = file.get<uint64_t>(0);
			const char *body = &file.getReference<char>(sizeof(uint64_t));

			// Find end of name
			size_t name_length = 0;
			while (name_length < body_size && body[name_length] != '\0') { name_length++; }

			const DefinitionExtended *extended =
				reinterpret_cast<const DefinitionExtended *>(body + name_length + 1);

			// Calculate CRC over body (name + extended fields excluding crc8)
			// CRC is over: name (with null) + magic + version + source_type + reserved
			const size_t crc_data_size = (name_length + 1) + sizeof(DefinitionExtended) - 1;

			// Use file's crc8 method
			const uint8_t calculated_crc = file.crc8(crc_data_size, sizeof(uint64_t));

			return calculated_crc == extended->crc8;
		}
	};
} // namespace CommonLowLevelTracingKit::decoder::source

#endif
