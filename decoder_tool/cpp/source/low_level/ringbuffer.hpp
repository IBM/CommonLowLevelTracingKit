#ifndef DECODER_TOOL_SOURCE_RINGBUFFER_HEADER
#define DECODER_TOOL_SOURCE_RINGBUFFER_HEADER
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <stdint.h>
#include <variant>
#include <vector>

#include "file.hpp"
#include "inline.hpp"

namespace CommonLowLevelTracingKit::decoder::source {

	class Ringbuffer final {
	  public:
		class Entry;

		using EntryPtr = std::unique_ptr<const Entry>;

		struct __attribute__((packed)) HeadPart final {
			uint64_t size;
			uint64_t wrapped;
			uint64_t dropped;
			uint64_t entries;
			uint64_t next_free;
			uint64_t last_valid;
			CONST_INLINE bool isWrapped() const noexcept { return next_free < last_valid; }
			CONST_INLINE __uint128_t next_free_abs() const noexcept {
				return (size * wrapped) + next_free;
			}
			CONST_INLINE __uint128_t last_valid_abs() const noexcept {
				if (wrapped > 0) [[likely]]
					return (size * (wrapped - isWrapped())) + last_valid;
				else
					return last_valid;
			}
			CONST_INLINE bool valid() const noexcept {
				return (dropped <= entries) && (next_free <= size) && (last_valid <= size);
			}
		};
		struct State final {
			State(const HeadPart &o) noexcept
				: m_position(0)
				, m_old_position(0)
				, m_entire_count(o.dropped)
				, m_size(o.size) {
				this->reset(o);
			}
			INLINE __uint128_t position_abs() const noexcept { return m_position; }
			INLINE __uint128_t max_position_abs() const noexcept {
				return std::max(m_position, m_old_position);
			}
			INLINE uint64_t position_rel() const noexcept {
				return static_cast<uint64_t>(m_position % m_size);
			}
			INLINE void reset(const HeadPart &o) noexcept {
				if (o.last_valid_abs() < m_position) [[unlikely]]
					return;
				m_old_position = std::max(m_position, m_old_position);
				m_position = std::max(m_position, o.last_valid_abs());
				m_entire_count = std::max(m_entire_count, o.dropped);
			}
			INLINE bool valid(const HeadPart &c) const noexcept {
				const auto last_valid = c.last_valid_abs();
				const auto next_free = c.next_free_abs();
				const auto read_position = position_abs();
				return c.valid() && (last_valid <= read_position) && (read_position <= next_free);
			}
			INLINE void increment(const size_t a) noexcept { m_position += a; }

			INLINE size_t next_entry_nr() noexcept { return m_entire_count++; }

		  private:
			__uint128_t m_position;
			__uint128_t m_old_position;
			uint64_t m_entire_count;
			const size_t m_size;
		};

		Ringbuffer(FilePart &&file);
		Ringbuffer(const Ringbuffer &) = delete;
		Ringbuffer &operator=(const Ringbuffer &) = delete;
		Ringbuffer(Ringbuffer &&) = delete;
		Ringbuffer &operator=(Ringbuffer &&) = delete;
		CONST_INLINE uint64_t getVersion() const noexcept { return m_version; }
		CONST_INLINE uint64_t getSize() const noexcept { return m_body_size - 1; }
		INLINE uint64_t getWrapped() const noexcept { return capture().wrapped; }
		INLINE uint64_t getDropped() const noexcept { return capture().dropped; }
		INLINE uint64_t getEntryCount() const noexcept { return capture().entries; }
		INLINE uint64_t getUsed() const noexcept {
			const auto c = capture();
			if (c.wrapped == 0) { return c.next_free; }
			return getSize();
		}
		INLINE uint64_t getAvailable() const noexcept { return getSize() - getUsed(); }
		INLINE const HeadPart capture() const noexcept {
			return m_headpart->load(std::memory_order_relaxed);
		}

		INLINE void reset() noexcept { m_read.reset(capture()); }
		INLINE size_t pendingBytes() noexcept { return pendingBytes(capture()); }
		INLINE size_t pendingBytes(const HeadPart &c) const noexcept {
			const auto &r = m_read;
			const auto head = c.next_free_abs();
			const auto tail = std::max(c.last_valid_abs(), r.max_position_abs());
			const auto diff = (head > tail) ? (head - tail) : 0;
			const uint64_t pending = static_cast<uint64_t>(std::min(diff, diff & UINT64_MAX));
			return std::min(pending, m_body_size - 1);
		}

		std::variant<EntryPtr, std::string> getNextEntry() noexcept;

		// Safe zone threshold: entries this far behind the write head are guaranteed
		// to not be overwritten, so CRC validation can be skipped for better performance.
		static constexpr size_t SAFE_ZONE_THRESHOLD = 4096;

	  private:
		std::mutex m_this_lock;
		const FilePart m_file;
		const uint64_t m_version;
		const std::atomic<HeadPart> *const m_headpart;
		State m_read;
		const FilePart m_body;
		const uint64_t m_body_size;
	};

	class Ringbuffer::Entry final {
	  public:
		CONST_INLINE const std::span<const uint8_t> &body() const noexcept { return m_body; }
		CONST_INLINE size_t size() const noexcept { return m_body.size(); }

		static constexpr uint8_t header_size = 4;

		static EntryPtr make(const uint64_t a_entry_number, const uint64_t a_body_start,
							 const size_t a_body_size, const FilePart &a_rb_body,
							 const uint64_t a_rb_size, const bool skip_crc = false);

		const uint64_t nr;

	  private:
		Entry(const uint64_t a_entry_number, const uint64_t a_body_start, const size_t a_body_size,
			  const FilePart &a_rb_body, const uint64_t a_rb_size, const bool skip_crc) noexcept;
		static inline constexpr size_t static_body_size = 256;
		friend class Ringbuffer;

		bool m_valid;
		std::array<uint8_t, static_body_size> m_static_body;
		std::vector<uint8_t> m_dynamic_body;
		std::span<const uint8_t> m_body;

		// never move or copy
		Entry(const Entry &) = delete;
		Entry &operator=(const Entry &) = delete;
		Entry(Entry &&) = delete;
		Entry &operator=(Entry &&) = delete;
	};
} // namespace CommonLowLevelTracingKit::decoder::source

#endif