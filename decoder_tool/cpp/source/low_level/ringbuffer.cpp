#include "ringbuffer.hpp"
#include "CommonLowLevelTracingKit/decoder/Common.hpp"
using namespace CommonLowLevelTracingKit::decoder::exception;
using namespace std::string_literals;

#include "crc8.hpp"

using Ringbuffer = CommonLowLevelTracingKit::decoder::source::Ringbuffer;
using Entry = Ringbuffer::Entry;
using EntryPtr = Ringbuffer::EntryPtr;

EntryPtr Entry::make(const uint64_t entry_nr, const uint64_t body_start, const size_t body_size,
					 const FilePart &rb_body, const uint64_t rb_size, const bool skip_crc) {
	Entry *raw = new Entry(entry_nr, body_start, body_size, rb_body, rb_size, skip_crc);
	return EntryPtr{raw};
}
Entry::Entry(const uint64_t entry_nr, const uint64_t body_start, const size_t body_size,
			 const FilePart &rb_body, const uint64_t rb_size, const bool skip_crc) noexcept
	: nr(entry_nr)
	, m_valid(false) {

	if (body_size <= static_body_size) [[likely]] {
		rb_body.copyOut(m_static_body, body_start, body_size, rb_size);
		m_body = std::span<const uint8_t>{m_static_body.data(), body_size};
	} else {
		m_dynamic_body.resize(body_size);
		rb_body.copyOut(m_dynamic_body, body_start, body_size, rb_size);
		m_body = std::span<const uint8_t>{m_dynamic_body.data(), body_size};
	}

	if (skip_crc) [[likely]] {
		// In safe zone - data is guaranteed stable, skip CRC validation
		m_valid = true;
	} else {
		const size_t body_crc_position = (body_start + body_size);
		const uint8_t body_crc = rb_body.getLimted<uint8_t>(rb_size, body_crc_position);
		m_valid = (body_crc == crc8(m_body));
	}
}

std::variant<EntryPtr, std::string> Ringbuffer::getNextEntry() noexcept {
	std::scoped_lock lock{m_this_lock};
	const size_t max_attemptes = std::max<size_t>(m_body_size, 10 * 1024UL);
	size_t retry_attempts = 0;
	while (true) {
		if ((++retry_attempts) > max_attemptes)
			return "could not syn ringbuffer in "s + __FILE__ + ":"s + std::to_string(__LINE__);
		const auto c = capture();
		[[unlikely]] if (!m_read.valid(c)) {
			m_read.reset(c);
			continue;
		}
		const size_t pending = pendingBytes(c);
		if (pending == 0) [[unlikely]] {
			// nothing new in ringbuffer
			return EntryPtr{nullptr};
		}
		// Skip CRC validation when far enough behind the write head (safe zone)
		const bool in_safe_zone = (pending > SAFE_ZONE_THRESHOLD);

		if (m_body.getLimted<uint8_t>(m_body_size, m_read.position_rel()) == '~') {
			// matches ringbuffer entry start
			const size_t entry_size =
				m_body.getLimted<uint16_t>(m_body_size, m_read.position_rel() + 1);
			if (entry_size >= std::numeric_limits<uint16_t>::max()) [[unlikely]] {
				// ringbuffer entry could not be so big
				m_read.increment(1);
				continue;
			}

			bool head_valid;
			if (in_safe_zone) [[likely]] {
				head_valid = true;
			} else {
				head_valid =
					(0 == m_body.crc8(Entry::header_size, m_read.position_rel(), m_body_size));
			}
			if (head_valid == false) [[unlikely]] {
				// ringbuffer entry header is not valid
				m_read.increment(1);
				continue;
			}

			const auto nr = m_read.next_entry_nr();
			const auto body_start = m_read.position_rel() + Entry::header_size;
			auto entry = Entry::make(nr, body_start, entry_size, m_body, m_body_size, in_safe_zone);
			if (!entry || !entry->m_valid) [[unlikely]] {
				// body of entry not valid
				m_read.increment(1);
				continue;
			}

			// If we skipped CRC validation (safe zone), re-check that our read position
			// is still valid after copying the data. The writer could have overwritten
			// our data between the initial capture() and the copyOut().
			if (in_safe_zone) [[likely]] {
				const auto c_after = capture();
				if (!m_read.valid(c_after)) [[unlikely]] {
					// Writer caught up - data may be corrupted, reset and retry
					m_read.reset(c_after);
					continue;
				}
			}

			// return valid entry
			m_read.increment(Entry::header_size + entry_size + 1);
			return entry;
		} else {
			m_read.increment(1);
		}
	}
}
Ringbuffer::Ringbuffer(FilePart &&file)
	: m_file(file)
	, m_version(file.get<uint64_t>(0))
	, m_headpart(&file.getReference<std::atomic<HeadPart>>(72))
	, m_read(capture())
	, m_body(file.get(160))
	, m_body_size(file.get<uint64_t>(72)) {}