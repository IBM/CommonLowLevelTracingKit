#include <memory>
#include <span>
#include <stdint.h>
#include <string.h>
#include <string>
#include <string_view>
#include <utility>

#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "TracepointInternal.hpp"
#include "file.hpp"
#include "formatter.hpp"
#include "ringbuffer.hpp"
#include "to_string.hpp"

using namespace CommonLowLevelTracingKit::decoder;
using ToString = CommonLowLevelTracingKit::decoder::source::low_level::ToString;
using namespace std::string_literals;

void TracepointDeleter::operator()(Tracepoint *ptr) const noexcept {
	if (ptr == nullptr) return;

	ptr->~Tracepoint(); // Call destructor

	if (m_pool != nullptr && m_dealloc != nullptr) {
		// Return to pool
		m_dealloc(m_pool, ptr);
	} else {
		// Heap allocation - use operator delete
		::operator delete(ptr);
	}
}

TraceEntryHead::TraceEntryHead(std::string_view tb_name, uint64_t n, uint64_t t,
							   const std::span<const uint8_t> &body, SourceType src)
	: Tracepoint(tb_name, n, t, src)
	, m_pid(get<uint32_t>(body, 6))
	, m_tid(get<uint32_t>(body, 10)) {}

TraceEntryHead::TraceEntryHead(std::string_view tb_name, uint64_t n, uint64_t t, uint32_t pid,
							   uint32_t tid, SourceType src)
	: Tracepoint(tb_name, n, t, src)
	, m_pid(pid)
	, m_tid(tid) {};

TracepointDynamic::TracepointDynamic(const std::string_view tb_name,
									 source::Ringbuffer::EntryPtr entry, SourceType src)
	: TraceEntryHead(tb_name, entry->nr, get<uint64_t>(entry->body(), 14), entry->body(), src)
	, e(std::move(entry)) {
	const size_t size = e->body().size();
	if (size < 22) {
		// Malformed entry - not enough data for header
		m_file = {};
		m_line = 0;
		m_msg = {};
		return;
	}
	const char *const begin = reinterpret_cast<const char *>(e->body().begin().base());
	const char *current = begin + 22;
	size_t remaining = size - 22;

	const char *const file_start = current;
	const size_t file_len = strnlen(file_start, remaining);
	m_file = std::string_view{file_start, file_len};
	if (remaining < file_len + 1) {
		m_line = 0;
		m_msg = {};
		return;
	}
	current += file_len + 1;
	remaining -= file_len + 1;

	const size_t line_len = sizeof(size_t);
	if (remaining < line_len) {
		m_line = 0;
		m_msg = {};
		return;
	}
	const char *const line_start = current;
	m_line = *std::bit_cast<const size_t *>(line_start);
	current += line_len;
	remaining -= line_len;

	const char *const msg_start = current;
	const size_t msg_len = strnlen(current, remaining);
	m_msg = std::string_view{msg_start, msg_len};
}

using FilePtr = source::internal::FilePtr;
TracepointStatic::TracepointStatic(const std::string_view tb_name,
								   source::Ringbuffer::EntryPtr &&entry,
								   const std::span<const uint8_t> &arg_m, const FilePtr &&f,
								   SourceType src)
	: TraceEntryHead(tb_name, entry->nr, get<uint64_t>(entry->body(), 14), entry->body(), src)
	, m(arg_m)
	, e(std::move(entry))
	, m_keep_memory(f)
	, m_type(toMetaType(get<uint8_t>(m, 5)))
	, m_line(get<uint32_t>(m, 6))
	, m_arg_count(std::min((uint8_t)10, get<uint8_t>(m, 10)))
	, m_arg_types((const char *)&m[11], m_arg_count)
	, m_file()
	, m_format() {}

const std::string_view TracepointStatic::file() const noexcept {
	if (m_file.empty()) {
		const size_t offset = 12 + m_arg_count;
		if (offset >= m.size()) {
			m_file = "";
		} else {
			m_file = std::string_view{std::bit_cast<char *>(&m[offset])};
		}
	}
	return m_file;
}

const std::string_view TracepointStatic::format() const noexcept {
	if (m_format.empty()) {
		const size_t offset = std::min(m.size(), (13 + m_arg_count + file().size()));
		const size_t size = m.size() - offset;
		if (size == 0)
			m_format = "";
		else
			m_format = std::string_view{std::bit_cast<char *>(&m[offset])};
	}
	return m_format;
}

const std::string_view TracepointStatic::msg() const {
	if (m_msg.empty()) {
		const uint8_t *const arg_start = (e->size() > 22) ? &e->body()[22] : nullptr;
		const size_t arg_size = (e->size() > 22) ? (e->size() - 22) : 0;
		std::span<const uint8_t> args_raw{arg_start, arg_size};
		if (m_type == MetaType::printf) [[likely]] {
			m_msg = source::formatter::printf(format(), m_arg_types, args_raw);
		} else if (m_type == MetaType::dump) {
			m_msg = source::formatter::dump(format(), m_arg_types, args_raw);
		} else {
			CLLTK_DECODER_THROW(
				exception::InvalidMeta,
				"Invalid meta data type: " + std::to_string(static_cast<uint8_t>(m_type)) +
					" (expected printf=1 or dump=2)");
		}
	}
	return m_msg;
}

std::string Tracepoint::date_and_time_str() const noexcept {
	return ToString::date_and_time(timestamp_ns);
}
std::string Tracepoint::timestamp_str() const noexcept {
	return ToString::timestamp_ns(timestamp_ns);
}
