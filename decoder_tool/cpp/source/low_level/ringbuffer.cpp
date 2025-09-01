#include "ringbuffer.hpp"

#include "crc8.hpp"

using Ringbuffer = CommonLowLevelTracingKit::decoder::source::Ringbuffer;
using Entry = Ringbuffer::Entry;
using EntryPtr = Ringbuffer::EntryPtr;

EntryPtr Entry::make(const uint64_t entry_nr, const uint64_t body_start, const size_t body_size,
					 const FilePart &rb_body, const uint64_t rb_size) {
	Entry *raw = new Entry(entry_nr, body_start, body_size, rb_body, rb_size);
	return EntryPtr{raw};
}
Entry::Entry(const uint64_t entry_nr, const uint64_t body_start, const size_t body_size,
			 const FilePart &rb_body, const uint64_t rb_size) noexcept
	: nr(entry_nr)
	, m_valid(false) {
	const size_t body_crc_position = (body_start + body_size);
	const uint8_t body_crc = rb_body.getLimted<uint8_t>(rb_size, body_crc_position);

	[[likely]] if (body_size <= static_body_size) {
		rb_body.copyOut(m_static_body, body_start, body_size, rb_size);
		m_body = std::span<const uint8_t>{m_static_body.data(), body_size};
	} else {
		m_dynmaic_body.resize(body_size);
		rb_body.copyOut(m_dynmaic_body, body_start, body_size, rb_size);
		m_body = std::span<const uint8_t>{m_dynmaic_body.data(), body_size};
	}

	m_valid = (body_crc == crc8(m_body));
	if (!m_valid) m_body = std::span<uint8_t>{};
}

EntryPtr Ringbuffer::getNextEntry() {
	std::scoped_lock lock{m_this_lock};
	size_t retry_attempts = 0;
	while (true) {
		if ((++retry_attempts) > 100) return nullptr;
		const auto c = capture();
		[[unlikely]] if (!m_read.valid(c)) {
			m_read.reset(c);
			continue;
		}

		[[unlikely]] if (pendingBytes(c) == 0) {
			return nullptr;
		} else if (m_body.getLimted<uint8_t>(m_body_size, m_read.position_rel()) == '~') {
			const size_t entry_size =
				m_body.getLimted<uint16_t>(m_body_size, m_read.position_rel() + 1);
			const bool head_valid =
				(0 == m_body.crc8(Entry::header_size, m_read.position_rel(), m_body_size));
			[[unlikely]] if (head_valid == false)
				goto next;

			auto entry =
				Entry::make(m_read.next_entry_nr(), m_read.position_rel() + Entry::header_size,
							entry_size, m_body, m_body_size);
			[[unlikely]] if (entry && entry->m_valid) {
				m_read.increment(Entry::header_size + entry_size + 1);
				return entry;
			}
			goto next;
		} else {
		next:
			m_read.increment(1);
			continue;
		}
	}
}
Ringbuffer::Ringbuffer(FilePart &&file)
	: m_file(file)
	, m_version(file.get<uint64_t>(0))
	, m_headpart(&file.getRef<std::atomic<HeadPart>>(72))
	, m_read(capture())
	, m_body(file.get(160))
	, m_body_size(file.get<uint64_t>(72)) {}