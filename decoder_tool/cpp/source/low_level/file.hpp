#ifndef DECODER_TOOL_SOURCE_FILE_HEADER
#define DECODER_TOOL_SOURCE_FILE_HEADER
#include "CommonLowLevelTracingKit/decoder/Inline.hpp"
#include <algorithm>
#include <array>
#include <bit>
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

		// byte-swap one arithmetic value; used to read files written on a
		// machine with the opposite byte order (detected via the file magic)
		template <typename T> INLINE T byteswapValue(T value) {
			static_assert(std::is_arithmetic_v<T>);
			if constexpr (sizeof(T) == 2) {
				return std::bit_cast<T>(__builtin_bswap16(std::bit_cast<uint16_t>(value)));
			} else if constexpr (sizeof(T) == 4) {
				return std::bit_cast<T>(__builtin_bswap32(std::bit_cast<uint32_t>(value)));
			} else if constexpr (sizeof(T) == 8) {
				return std::bit_cast<T>(__builtin_bswap64(std::bit_cast<uint64_t>(value)));
			} else {
				return value;
			}
		}

		template <typename T>
		concept ByteSwappable = std::is_arithmetic_v<T> && (sizeof(T) > 1);
	} // namespace internal
	class FilePart final {
		friend struct internal::File;

	  public:
		FilePart(const std::filesystem::path &);

		FilePart(const FilePart &) noexcept = default;
		FilePart(FilePart &&) noexcept = default;
		FilePart &operator=(const FilePart &) = delete;
		FilePart &operator=(FilePart &&) = delete;

		/// raw zero-copy access; never byte-swapped. Use only for byte arrays
		/// and for native-endian live paths (foreign-endian files are
		/// restricted to offline decoding).
		template <internal::POD T> INLINE const T &getReference(size_t offset = 0) const {
			const T &value = *(const T *)getPtr(offset, sizeof(T));
			return value;
		}
		template <typename T = FilePart> INLINE T get(size_t offset = 0) const {
			// trace data is byte-packed, so the source may be unaligned for T;
			// copy the bytes out instead of a misaligned load
			T value;
			std::memcpy(&value, reinterpret_cast<const void *>(getPtr(offset, sizeof(T))),
						sizeof(T));
			if constexpr (internal::ByteSwappable<T>) {
				if (m_foreign_endian) { value = internal::byteswapValue(value); }
			}
			return value;
		}

		template <typename T>
			requires(internal::POD<T>)
		INLINE T getLimted(size_t limit, size_t offset = 0) const noexcept {
			std::array<uint8_t, sizeof(T)> raw = {};
			getLimtedRaw(raw.data(), offset, raw.size(), limit);
			T value = *reinterpret_cast<const T *>(raw.data());
			if constexpr (internal::ByteSwappable<T>) {
				if (m_foreign_endian) { value = internal::byteswapValue(value); }
			}
			return value;
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

		/// mark this file as written with the opposite byte order (decided by
		/// the tracebuffer magic); all get/getLimted calls then byte-swap.
		/// Sub-FileParts created afterwards inherit the flag.
		INLINE void setForeignEndian(bool foreign) noexcept { m_foreign_endian = foreign; }
		INLINE bool isForeignEndian() const noexcept { return m_foreign_endian; }

	  private:
		uintptr_t getPtr(size_t offset, size_t object_size) const;
		void getLimtedRaw(uint8_t *const out, size_t offset, size_t size,
						  size_t limit) const noexcept;
		FilePart(const FilePart &a_filePart, size_t a_offset);
		const internal::FilePtr m_file;
		const size_t m_offset;
		const uintptr_t m_base;
		bool m_foreign_endian = false;
	};

	template <> FilePart FilePart::get<FilePart>(size_t offset) const;

} // namespace CommonLowLevelTracingKit::decoder::source

#endif