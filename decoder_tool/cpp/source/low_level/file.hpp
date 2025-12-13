#ifndef DECODER_TOOL_SOURCE_FILE_HEADER
#define DECODER_TOOL_SOURCE_FILE_HEADER
#include "CommonLowLevelTracingKit/decoder/Inline.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <type_traits>

namespace CommonLowLevelTracingKit::decoder::source {
	namespace internal {
		template <typename T>
		concept POD = (std::is_standard_layout_v<T>);
		struct File;
		using FilePtr = std::shared_ptr<File>;

		static constexpr size_t s_max_file_size = 1024 * 1024 * 1024;
	} // namespace internal
	class FilePart final {
		friend struct internal::File;

	  public:
		FilePart(const std::filesystem::path &);

		FilePart(const FilePart &) noexcept = default;
		FilePart(FilePart &&) noexcept = default;
		FilePart &operator=(const FilePart &) = delete;
		FilePart &operator=(FilePart &&) = delete;

		template <internal::POD T> INLINE const T &getReference(size_t offset = 0) const {
			const T &value = *(const T *)getPtr(offset, sizeof(T));
			return value;
		}
		template <typename T = FilePart> INLINE T get(size_t offset = 0) const {
			return *(const T *)getPtr(offset, sizeof(T));
		}

		template <typename T>
			requires(internal::POD<T>)
		INLINE T getLimted(size_t limit, size_t offset = 0) const noexcept {
			std::array<uint8_t, sizeof(T)> value = {};
			getLimtedRaw(value.data(), offset, value.size(), limit);
			return *reinterpret_cast<const T *>(value.data());
		}
		template <std::ranges::contiguous_range R>
		INLINE void copyOut(R &out, size_t offset, size_t size, size_t limit) const {
			using T = std::ranges::range_value_t<R>;
			uint8_t *const data = reinterpret_cast<uint8_t *>(std::ranges::data(out));
			const auto n = std::ranges::size(out);
			const size_t calculated_size = (sizeof(T) * n);
			getLimtedRaw(data, offset, std::min(size, calculated_size), limit);
		}

		uint8_t crc8(size_t size, size_t offset = 0, size_t limit = 0) const noexcept;

		size_t getFileSize() const;
		size_t grow() const;

		INLINE internal::FilePtr getFileInternal() const noexcept { return m_file; }

		const std::filesystem::path &path() const noexcept;

	  private:
		uintptr_t getPtr(size_t offset, size_t object_size) const;
		void getLimtedRaw(uint8_t *const out, size_t offset, size_t size,
						  size_t limit) const noexcept;
		FilePart(const FilePart &a_filePart, size_t a_offset);
		const internal::FilePtr m_file;
		const size_t m_offset;
		const uintptr_t m_base;
	};

	template <> FilePart FilePart::get<FilePart>(size_t offset) const;

} // namespace CommonLowLevelTracingKit::decoder::source

#endif