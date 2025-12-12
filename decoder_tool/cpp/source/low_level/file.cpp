
#include "file.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>  // errno
#include <cstring> // strerror
#include <fcntl.h> // open
#include <mutex>
#include <span>
#include <stdexcept>  // for std::runtime_error
#include <sys/mman.h> // mmap, munmap
#include <sys/stat.h> // fstat
#include <unistd.h>	  // close

#include "CommonLowLevelTracingKit/decoder/Inline.hpp"
#include "crc8.hpp"

namespace source = CommonLowLevelTracingKit::decoder::source;
namespace internal = CommonLowLevelTracingKit::decoder::source::internal;
using namespace std::string_literals;

struct internal::File final {
	File(const std::filesystem::path &);
	~File();
	File(File &&) = delete;
	File(const File &) = delete;
	File &operator=(const File &) = delete;
	File &operator=(File &&) = delete;

	size_t grow() const;

	inline size_t size() const { return m_size; }
	inline uintptr_t data() const { return m_base; }
	inline const std::filesystem::path &path() const { return m_path; }

  private:
	size_t getRealSize() const;
	mutable std::mutex m_mutex;
	const std::filesystem::path m_path;
	int m_fd;
	uintptr_t m_base;
	mutable std::atomic<size_t> m_size;
	bool m_moved;
};

internal::File::File(const std::filesystem::path &a_path)
	: m_path(a_path)
	, m_moved(false) {
	std::scoped_lock lock(m_mutex);
	m_fd = open(a_path.c_str(), O_RDONLY);
	if (m_fd == -1) { throw std::runtime_error("Error opening file: "s + strerror(errno)); }

	m_base = (uintptr_t)mmap(nullptr, s_max_file_size, PROT_NONE, MAP_SHARED, m_fd, 0);
	if (m_base == (uintptr_t)MAP_FAILED) {
		throw std::runtime_error("Error mapping file: "s + strerror(errno));
	}
}

internal::File::~File() {
	std::scoped_lock lock(m_mutex);
	if (!m_moved) {
		if (m_base && m_base != (uintptr_t)MAP_FAILED) { munmap((void *)m_base, s_max_file_size); }
		m_base = 0;
		m_size = 0;
		if (m_fd >= 0) {
			close(m_fd);
			m_fd = -1;
		}
	}
}

size_t internal::File::getRealSize() const {
	if (m_fd < 0) { return 0; }
	struct stat sb;
	if (fstat(m_fd, &sb) == -1) {
		throw std::runtime_error("Error getting file size: "s + strerror(errno));
	}
	return safe_cast<size_t>(sb.st_size);
}

size_t internal::File::grow() const {
	std::scoped_lock lock(m_mutex);
	const size_t new_file_size = getRealSize();
	if (new_file_size == m_size) { return m_size; }
	if (mprotect((void *)m_base, new_file_size, PROT_READ) == -1) {
		throw std::runtime_error("Error mapping file: "s + strerror(errno));
	}
	if (madvise((void *)m_base, new_file_size, MADV_SEQUENTIAL | MAP_POPULATE) == -1) {}
	m_size = new_file_size;
	return m_size;
}

source::FilePart::FilePart(const FilePart &a_filePart, const size_t a_offset)
	: m_file(a_filePart.m_file)
	, m_offset(a_filePart.m_offset + a_offset)
	, m_base(m_file->data()) {
	const size_t max_access = m_offset;
	if (max_access >= m_file->size()) [[unlikely]] {
		m_file->grow();
		if (max_access >= m_file->size()) {
			throw std::out_of_range("out of file access (" + std::to_string(max_access) + ") in " +
									m_file->path().string());
		}
	}
}

uintptr_t source::FilePart::getPtr(size_t offset, size_t object_size) const {
	const size_t max_access = m_offset + offset + object_size - 1;
	if (max_access >= m_file->size()) [[unlikely]] {
		m_file->grow();
		if (max_access >= m_file->size()) {
			throw std::out_of_range("out of file access (" + std::to_string(max_access) + ") in " +
									m_file->path().string());
		}
	}
	return m_base + m_offset + offset;
}

void source::FilePart::getLimtedRaw(uint8_t *const out, size_t offset, size_t size,
									size_t limit) const noexcept {
	offset %= limit;
	const size_t first_part_size = std::min(size, limit - offset);
	const uintptr_t first_part_start = m_base + m_offset + offset;
	std::memcpy(out, (void *)first_part_start, first_part_size);
	if (first_part_size == size) return;
	const size_t second_part_size = std::min(size - first_part_size, limit);
	const uintptr_t second_part_start = m_base + m_offset;
	std::memcpy(&out[first_part_size], (void *)second_part_start, second_part_size);
}

template <> source::FilePart source::FilePart::get<source::FilePart>(size_t offset) const {
	return FilePart(*this, offset);
}

source::FilePart::FilePart(const std::filesystem::path &a_path)
	: m_file(std::make_shared<internal::File>(a_path))
	, m_offset(0)
	, m_base(m_file->data()) {
	m_file->grow();
}

size_t source::FilePart::getFileSize() const {
	return m_file->size();
}
size_t source::FilePart::grow() const {
	return m_file->grow();
}
uint8_t source::FilePart::crc8(size_t size, size_t offset, size_t limit) const noexcept {
	const uint8_t *const first_part_start =
		std::bit_cast<const uint8_t *>(m_base + m_offset + offset);
	if (limit == 0 || ((offset + size) <= limit)) {
		const std::span<const uint8_t> span{first_part_start, size};
		return source::crc8(span);
	} else {
		const size_t first_part_size = std::min(size, limit - offset);
		const std::span<const uint8_t> span1{first_part_start, first_part_size};
		uint8_t crc = source::crc8(span1);
		const size_t second_part_size = size - first_part_size;
		const uint8_t *const second_part_start =
			reinterpret_cast<const uint8_t *>(m_base + m_offset);
		const std::span<const uint8_t> span2{second_part_start, second_part_size};
		crc = source::crc8(span2, crc);
		return crc;
	}
}

const std::filesystem::path &source::FilePart::path() const noexcept {
	return m_file->path();
}