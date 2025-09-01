#include <memory>
#include <span>
#include <stdint.h>
#include <string.h>
#include <string>
#include <string_view>
#include <utility>

#include "CommonLowLevelTracingKit/Decoder/Tracepoint.hpp"
#include "TracepointInternal.hpp"
#include "file.hpp"
#include "formatter.hpp"
#include "ringbuffer.hpp"
#include "to_string.hpp"

using namespace CommonLowLevelTracingKit::decoder;
using ToString = CommonLowLevelTracingKit::decoder::source::low_level::ToString;

TraceEntryHead::TraceEntryHead(const std::span<const uint8_t> &body)
	: m_pid(get<uint32_t>(body, 6))
	, m_tid(get<uint32_t>(body, 10)) {}

TracepointDynamic::TracepointDynamic(const std::string_view &tb, source::Ringbuffer::EntryPtr a_e)
	: Tracepoint(tb, a_e->nr, get<uint64_t>(a_e->body(), 14))
	, TraceEntryHead(a_e->body())
	, e(std::move(a_e)) {
	const size_t size = e->body().size();
	const char *const begin = reinterpret_cast<const char *>(e->body().begin().base());
	const char *current = begin + 22;
	size_t remaining = size - 22;

	const char *const file_start = current;
	const size_t file_len = strnlen(file_start, remaining);
	m_file = std::string_view{file_start, file_len};
	current += file_len + 1;
	remaining -= file_len + 1;

	const char *const line_start = current;
	const size_t line_len = sizeof(size_t);
	m_line = *reinterpret_cast<const size_t *>(line_start);
	current += line_len;
	remaining -= line_len;

	const char *const msg_start = current;
	const size_t msg_len = strnlen(current, remaining);
	m_msg = std::string_view{msg_start, msg_len};
	current += msg_len + 1;
	remaining -= msg_len + 1;
	(void)remaining;
	(void)current;
}

using FilePtr = source::internal::FilePtr;
TracepointStatic::TracepointStatic(const std::string_view &tb, source::Ringbuffer::EntryPtr &&a_e,
								   const std::span<const uint8_t> &m, const FilePtr &&f)
	: Tracepoint(tb, a_e->nr, get<uint64_t>(a_e->body(), 14))
	, TraceEntryHead(a_e->body())
	, e(std::move(a_e))
	, m_keep_memory(f)
	, m_type(toMetaType(get<uint8_t>(m, 5)))
	, m_line(get<uint32_t>(m, 6))
	, m_arg_count(get<uint8_t>(m, 10))
	, m_arg_types((const char *)&m[11], m_arg_count)
	, m_file((const char *)&m[12 + m_arg_count])
	, m_format((const char *)&m[13 + m_arg_count + m_file.size()]) {}

const std::string_view TracepointStatic::msg() const {
	if (m_msg.empty()) {
		std::span<const uint8_t> args_raw{&e->body()[22], e->size() - 22};
		if (m_type == MetaType::printf) [[likely]]
			m_msg = source::formatter::printf(m_format, m_arg_types, args_raw);
		else if (m_type == MetaType::dump)
			m_msg = source::formatter::dump(m_format, m_arg_types, args_raw);
		else
			m_msg = "<invalid meta data>";
	}
	return m_msg;
}
std::string Tracepoint::date_and_time_str() const noexcept {
	return ToString::date_and_time(timestamp_ns);
}
std::string Tracepoint::timestamp_str() const noexcept {
	return ToString::timestamp_ns(timestamp_ns);
}
