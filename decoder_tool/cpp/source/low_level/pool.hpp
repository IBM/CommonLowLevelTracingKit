// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef DECODER_TOOL_SOURCE_POOL_HEADER
#define DECODER_TOOL_SOURCE_POOL_HEADER

#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include "inline.hpp"

namespace CommonLowLevelTracingKit::decoder::source {

	/**
	 * @brief Lock-free memory pool for fixed-size objects
	 *
	 * Design:
	 * - Lock-free allocation and deallocation using atomic free list
	 * - Grows by allocating new blocks (mutex protected, rare operation)
	 * - Each slot is SlotSize bytes
	 * - Never returns memory to OS (blocks kept until pool destruction)
	 *
	 * Thread safety:
	 * - allocate(): lock-free (unless grow needed)
	 * - deallocate(): lock-free
	 * - grow is mutex-protected
	 */
	template <size_t SlotSize, size_t BlockSlots = 1024> class MemoryPool {
		static_assert(SlotSize >= sizeof(void *), "SlotSize must be at least pointer size");
		static_assert((SlotSize % alignof(std::max_align_t)) == 0,
					  "SlotSize must be aligned to max_align_t");

	  public:
		static constexpr size_t slot_size = SlotSize;
		static constexpr size_t block_slots = BlockSlots;

		explicit MemoryPool(size_t initial_blocks = 1) {
			for (size_t i = 0; i < initial_blocks; ++i) { grow_locked(); }
		}

		~MemoryPool() {
			// Blocks are freed automatically via unique_ptr in m_blocks
		}

		// Non-copyable, non-movable
		MemoryPool(const MemoryPool &) = delete;
		MemoryPool &operator=(const MemoryPool &) = delete;
		MemoryPool(MemoryPool &&) = delete;
		MemoryPool &operator=(MemoryPool &&) = delete;

		/**
		 * @brief Allocate a slot from the pool (lock-free fast path)
		 * @return Pointer to SlotSize bytes, or nullptr if allocation fails
		 */
		[[nodiscard]] INLINE void *allocate() noexcept {
			// Try lock-free pop from free list
			Node *old_head = m_free_list.load(std::memory_order_acquire);
			while (old_head != nullptr) {
				Node *new_head = old_head->next;
				if (m_free_list.compare_exchange_weak(old_head, new_head, std::memory_order_release,
													  std::memory_order_acquire)) {
					m_allocated.fetch_add(1, std::memory_order_relaxed);
					return static_cast<void *>(old_head);
				}
				// CAS failed, old_head is updated, retry
			}

			// Free list empty, need to grow (slow path)
			return allocate_slow();
		}

		/**
		 * @brief Return a slot to the pool (lock-free)
		 * @param ptr Pointer previously returned by allocate()
		 */
		INLINE void deallocate(void *ptr) noexcept {
			if (ptr == nullptr) return;

			Node *node = static_cast<Node *>(ptr);
			Node *old_head = m_free_list.load(std::memory_order_relaxed);
			do {
				node->next = old_head;
			} while (!m_free_list.compare_exchange_weak(old_head, node, std::memory_order_release,
														std::memory_order_relaxed));
			m_allocated.fetch_sub(1, std::memory_order_relaxed);
		}

		/**
		 * @brief Get current number of allocated slots
		 */
		[[nodiscard]] size_t allocated() const noexcept {
			return m_allocated.load(std::memory_order_relaxed);
		}

		/**
		 * @brief Get total capacity (allocated + free)
		 */
		[[nodiscard]] size_t capacity() const noexcept {
			return m_capacity.load(std::memory_order_relaxed);
		}

	  private:
		// Free list node - stored in-place in free slots
		struct Node {
			Node *next;
		};

		// Memory block
		struct Block {
			alignas(std::max_align_t) std::byte data[SlotSize * BlockSlots];
		};

		std::atomic<Node *> m_free_list{nullptr};
		std::atomic<size_t> m_allocated{0};
		std::atomic<size_t> m_capacity{0};
		std::mutex m_grow_mutex;
		std::vector<std::unique_ptr<Block>> m_blocks;

		/**
		 * @brief Slow path: grow pool and allocate
		 */
		[[nodiscard]] void *allocate_slow() noexcept {
			std::lock_guard lock(m_grow_mutex);

			// Double-check: someone else might have grown while we waited
			Node *head = m_free_list.load(std::memory_order_acquire);
			if (head != nullptr) {
				// Try to pop again
				while (head != nullptr) {
					Node *new_head = head->next;
					if (m_free_list.compare_exchange_weak(head, new_head, std::memory_order_release,
														  std::memory_order_acquire)) {
						m_allocated.fetch_add(1, std::memory_order_relaxed);
						return static_cast<void *>(head);
					}
				}
			}

			// Actually need to grow
			if (!grow_locked()) {
				return nullptr; // Allocation failed
			}

			// Now we definitely have slots, try again
			head = m_free_list.load(std::memory_order_acquire);
			while (head != nullptr) {
				Node *new_head = head->next;
				if (m_free_list.compare_exchange_weak(head, new_head, std::memory_order_release,
													  std::memory_order_acquire)) {
					m_allocated.fetch_add(1, std::memory_order_relaxed);
					return static_cast<void *>(head);
				}
			}

			return nullptr; // Should not happen
		}

		/**
		 * @brief Grow the pool by one block (must hold m_grow_mutex)
		 * @return true on success
		 */
		bool grow_locked() {
			auto block = std::make_unique<Block>();

			// Add all slots from this block to free list
			std::byte *base = block->data;
			for (size_t i = 0; i < BlockSlots; ++i) {
				void *slot = base + (i * SlotSize);
				Node *node = static_cast<Node *>(slot);

				// Push to free list
				Node *old_head = m_free_list.load(std::memory_order_relaxed);
				do {
					node->next = old_head;
				} while (!m_free_list.compare_exchange_weak(
					old_head, node, std::memory_order_release, std::memory_order_relaxed));
			}

			m_capacity.fetch_add(BlockSlots, std::memory_order_relaxed);
			m_blocks.push_back(std::move(block));
			return true;
		}
	};

	/**
	 * @brief Deleter for use with unique_ptr that returns memory to a pool
	 */
	template <typename Pool> class PoolDeleter {
	  public:
		explicit PoolDeleter(Pool *pool = nullptr) noexcept
			: m_pool(pool) {}

		template <typename T> void operator()(T *ptr) const noexcept {
			if (ptr) {
				ptr->~T(); // Call destructor
				if (m_pool) { m_pool->deallocate(ptr); }
			}
		}

		Pool *pool() const noexcept { return m_pool; }

	  private:
		Pool *m_pool;
	};

	/**
	 * @brief Allocate and construct an object in the pool
	 */
	template <typename T, typename Pool, typename... Args>
	[[nodiscard]] auto make_pooled(Pool &pool, Args &&...args) {
		using Deleter = PoolDeleter<Pool>;
		using Ptr = std::unique_ptr<T, Deleter>;

		static_assert(sizeof(T) <= Pool::slot_size, "Object size exceeds pool slot size");
		static_assert(alignof(T) <= alignof(std::max_align_t),
					  "Object alignment exceeds max_align_t");

		void *mem = pool.allocate();
		if (!mem) { return Ptr(nullptr, Deleter(&pool)); }

		try {
			T *obj = new (mem) T(std::forward<Args>(args)...);
			return Ptr(obj, Deleter(&pool));
		} catch (...) {
			pool.deallocate(mem);
			throw;
		}
	}

	// ============================================================================
	// Tracepoint Pool Configuration
	// ============================================================================

	/**
	 * @brief Slot size for tracepoints (must fit largest tracepoint type)
	 *
	 * TracepointStatic is the largest at 192 bytes.
	 * We round up to 256 to allow for future growth and better alignment.
	 */
	static constexpr size_t TRACEPOINT_SLOT_SIZE = 256;
	static constexpr size_t TRACEPOINT_BLOCK_SLOTS = 1024;

	/**
	 * @brief Memory pool for tracepoint allocation
	 *
	 * Inherits from MemoryPool to be a concrete class (not a type alias)
	 * for proper forward declaration support.
	 */
	class TracepointPool : public MemoryPool<TRACEPOINT_SLOT_SIZE, TRACEPOINT_BLOCK_SLOTS> {
	  public:
		using Base = MemoryPool<TRACEPOINT_SLOT_SIZE, TRACEPOINT_BLOCK_SLOTS>;
		using Base::Base; // Inherit constructors
	};

	// ============================================================================
	// String Pool for Small String Optimization
	// ============================================================================

	/**
	 * @brief Fixed-size string buffer for small string optimization
	 *
	 * Used for tracepoint messages that fit within the buffer size.
	 * Larger strings fall back to heap allocation.
	 */
	static constexpr size_t STRING_SLOT_SIZE = 256;
	static constexpr size_t STRING_BLOCK_SLOTS = 2048;

	using StringPool = MemoryPool<STRING_SLOT_SIZE, STRING_BLOCK_SLOTS>;

	/**
	 * @brief Pooled string with small string optimization
	 *
	 * If the string fits in STRING_SLOT_SIZE, it uses pool memory.
	 * Otherwise, it allocates from heap.
	 */
	class PooledString {
	  public:
		PooledString() noexcept = default;

		PooledString(StringPool *pool, std::string_view str)
			: m_pool(pool) {
			assign(str);
		}

		PooledString(StringPool *pool, std::string &&str)
			: m_pool(pool) {
			assign(std::move(str));
		}

		~PooledString() { clear(); }

		// Move constructor
		PooledString(PooledString &&other) noexcept
			: m_pool(other.m_pool)
			, m_data(other.m_data)
			, m_size(other.m_size)
			, m_uses_pool(other.m_uses_pool) {
			other.m_data = nullptr;
			other.m_size = 0;
			other.m_uses_pool = false;
		}

		// Move assignment
		PooledString &operator=(PooledString &&other) noexcept {
			if (this != &other) {
				clear();
				m_pool = other.m_pool;
				m_data = other.m_data;
				m_size = other.m_size;
				m_uses_pool = other.m_uses_pool;
				other.m_data = nullptr;
				other.m_size = 0;
				other.m_uses_pool = false;
			}
			return *this;
		}

		// No copy
		PooledString(const PooledString &) = delete;
		PooledString &operator=(const PooledString &) = delete;

		void assign(std::string_view str) {
			clear();
			if (str.empty()) return;

			m_size = str.size();
			if (m_pool != nullptr && str.size() < STRING_SLOT_SIZE) {
				// Use pool
				m_data = static_cast<char *>(m_pool->allocate());
				if (m_data != nullptr) {
					std::memcpy(m_data, str.data(), str.size());
					m_data[str.size()] = '\0';
					m_uses_pool = true;
					return;
				}
			}
			// Fall back to heap
			m_data = new char[str.size() + 1];
			std::memcpy(m_data, str.data(), str.size());
			m_data[str.size()] = '\0';
			m_uses_pool = false;
		}

		void assign(std::string &&str) {
			// For rvalue strings, just copy (we can't steal std::string's buffer easily)
			assign(std::string_view{str});
		}

		void clear() noexcept {
			if (m_data != nullptr) {
				if (m_uses_pool && m_pool != nullptr) {
					m_pool->deallocate(m_data);
				} else {
					delete[] m_data;
				}
				m_data = nullptr;
				m_size = 0;
				m_uses_pool = false;
			}
		}

		[[nodiscard]] std::string_view view() const noexcept {
			if (m_data == nullptr) return {};
			return {m_data, m_size};
		}

		[[nodiscard]] const char *c_str() const noexcept {
			if (m_data == nullptr) return "";
			return m_data;
		}

		[[nodiscard]] size_t size() const noexcept { return m_size; }
		[[nodiscard]] bool empty() const noexcept { return m_size == 0; }
		[[nodiscard]] bool uses_pool() const noexcept { return m_uses_pool; }

		operator std::string_view() const noexcept { return view(); }

	  private:
		StringPool *m_pool{nullptr};
		char *m_data{nullptr};
		size_t m_size{0};
		bool m_uses_pool{false};
	};

	/**
	 * @brief Global string pool singleton
	 *
	 * Thread-safe access to a shared string pool for tracepoint messages.
	 */
	class GlobalStringPool {
	  public:
		static StringPool &instance() {
			static StringPool pool{4}; // Start with 4 blocks = 8192 string slots
			return pool;
		}

		// Convenience method to create a pooled string
		static PooledString make(std::string_view str) { return PooledString{&instance(), str}; }

	  private:
		GlobalStringPool() = delete;
	};

} // namespace CommonLowLevelTracingKit::decoder::source

#endif
