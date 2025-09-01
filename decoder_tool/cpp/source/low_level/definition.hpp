#ifndef DECODER_TOOL_SOURCE_DEFINITION_HEADER
#define DECODER_TOOL_SOURCE_DEFINITION_HEADER
#include "file.hpp"
#include "inline.hpp"
#include <string_view>
namespace CommonLowLevelTracingKit::decoder::source {
	class Definition {
	  public:
		Definition(const FilePart &&a_file)
			: m_file(a_file)
			, m_name(std::string_view(&a_file.getRef<char>(sizeof(size_t)), a_file.get<size_t>())) {
			};
		virtual ~Definition() = default;
		Definition(const Definition &) = delete;
		Definition &operator=(const Definition &) = delete;
		Definition(Definition &&) = default;
		Definition &operator=(Definition &&) = delete;
		CONST_INLINE const std::string_view name() const { return m_name; }
		CONST_INLINE bool isValid() const {
			return !m_name.empty() && m_name.size() < m_file.getFileSize();
		}

	  private:
		const FilePart m_file;
		const std::string_view m_name;
	};
} // namespace CommonLowLevelTracingKit::decoder::source

#endif