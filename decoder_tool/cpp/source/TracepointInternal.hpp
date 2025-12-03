#ifndef DECODER_TOOL_SOURCE_TRACEPOINT_INTERNAL_HEADER
#define DECODER_TOOL_SOURCE_TRACEPOINT_INTERNAL_HEADER
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "file.hpp"
#include "inline.hpp"
#include "pool.hpp"
#include "ringbuffer.hpp"
#include <new>
#include <ranges>
#include <type_traits>

template <typename T>
concept PODType = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

template <PODType T, std::ranges::contiguous_range R>
INLINE static constexpr T get(R r, size_t offset = 0) {
	const uintptr_t base = std::bit_cast<uintptr_t>(r.data());
	const uintptr_t position = base + offset;
	const T *const ptr = reinterpret_cast<T *>(position);
	return *ptr;
}

namespace CommonLowLevelTracingKit::decoder {

	/**
	 * @brief Create a heap-allocated TracepointPtr (uses default deleter behavior)
	 */
	template <typename T, typename... Args>
	[[nodiscard]] INLINE TracepointPtr make_tracepoint(Args &&...args) {
		T *raw = new T(std::forward<Args>(args)...);
		return TracepointPtr(raw, TracepointDeleter{});
	}

	/**
	 * @brief Deallocator function for tracepoint pool (used by TracepointDeleter)
	 */
	inline void tracepoint_pool_deallocate(void *pool, void *ptr) noexcept {
		static_cast<source::TracepointPool *>(pool)->deallocate(ptr);
	}

	/**
	 * @brief Create a pool-allocated TracepointPtr
	 *
	 * Allocates from the given pool. If pool allocation fails, falls back to heap.
	 */
	template <typename T, typename... Args>
	[[nodiscard]] INLINE TracepointPtr make_pooled_tracepoint(source::TracepointPool &pool,
															  Args &&...args) {
		static_assert(sizeof(T) <= source::TRACEPOINT_SLOT_SIZE,
					  "Tracepoint type too large for pool");

		void *mem = pool.allocate();
		if (mem != nullptr) {
			try {
				T *obj = new (mem) T(std::forward<Args>(args)...);
				return TracepointPtr(obj, TracepointDeleter{&pool, tracepoint_pool_deallocate});
			} catch (...) {
				pool.deallocate(mem);
				throw;
			}
		}

		// Fallback to heap allocation
		return make_tracepoint<T>(std::forward<Args>(args)...);
	}

	struct TraceEntryHead : public Tracepoint {
		const uint32_t m_pid;
		const uint32_t m_tid;
		TraceEntryHead(std::string_view tb_name, uint64_t n, uint64_t t,
					   const std::span<const uint8_t> &body, SourceType src = SourceType::Unknown);
		TraceEntryHead(std::string_view tb_name, uint64_t n, uint64_t t, uint32_t pid, uint32_t tid,
					   SourceType src = SourceType::Unknown);
		uint32_t pid() const noexcept override { return m_pid; };
		uint32_t tid() const noexcept override { return m_tid; };
	};

	class TracepointDynamic final : public TraceEntryHead {
	  public:
		~TracepointDynamic() = default;
		TracepointDynamic(const std::string_view tb_name, source::Ringbuffer::EntryPtr entry,
						  SourceType src = SourceType::Unknown);

		Type type() const noexcept override { return Type::Dynamic; }
		const std::string_view file() const noexcept override { return m_file; }
		uint64_t line() const noexcept override { return m_line; }
		const std::string_view msg() const override { return m_msg; }

	  private:
		const std::string_view m_tracebuffer;
		const source::Ringbuffer::EntryPtr e;
		std::string_view m_file;
		size_t m_line;
		std::string_view m_msg;
	};

	enum class MetaType : uint8_t { undefined = 0, printf = 1, dump = 2 };
	static constexpr CONST_INLINE MetaType toMetaType(uint8_t a) {
		if (a <= static_cast<uint8_t>(MetaType::dump))
			return static_cast<MetaType>(a);
		else
			return MetaType::undefined;
	}
	class TracepointStatic final : public TraceEntryHead {
	  public:
		~TracepointStatic() = default;
		TracepointStatic(const std::string_view tb_name, source::Ringbuffer::EntryPtr &&entry,
						 const std::span<const uint8_t> &meta, const source::internal::FilePtr &&,
						 SourceType src = SourceType::Unknown);

		Type type() const noexcept override { return Type::Static; }
		const std::string_view file() const noexcept override;
		uint64_t line() const noexcept override { return m_line; }
		const std::string_view msg() const override;

	  private:
		const std::string_view format() const noexcept;
		const std::span<const uint8_t> m;

		const source::Ringbuffer::EntryPtr e;
		const source::internal::FilePtr m_keep_memory; // keep meta memory
		const MetaType m_type;
		const uint32_t m_line;
		const uint8_t m_arg_count;
		const std::span<const char> m_arg_types;
		mutable std::string_view m_file;
		mutable std::string_view m_format;
		mutable std::string m_msg;
	};

	struct VirtualTracepoint : public TraceEntryHead {
		VirtualTracepoint(const std::string tb_name, const std::string &msg,
						  SourceType src = SourceType::Unknown)
			: TraceEntryHead(tb_name, 0, 0, 0, 0, src)
			, m_tracebuffer(std::move(tb_name))
			, m_msg(msg)
			, m_file()
			, m_line() {}
		VirtualTracepoint(const std::string tb_name, const source::Ringbuffer::Entry &e,
						  const std::string &msg, SourceType src = SourceType::Unknown)
			: TraceEntryHead(tb_name, e.nr, get<uint64_t>(e.body(), 14), 0, 0, src)
			, m_tracebuffer(std::move(tb_name))
			, m_msg(msg)
			, m_file()
			, m_line() {}

		~VirtualTracepoint() = default;
		Type type() const noexcept override { return Type::Virtual; }
		const std::string_view file() const noexcept override { return m_file; }
		uint64_t line() const noexcept override { return m_line; }
		const std::string_view msg() const override { return m_msg; }

		template <class... Args>
		static INLINE auto make(const std::string_view tb_name, Args &&...args) {
			return make_tracepoint<VirtualTracepoint>(std::string{tb_name.data(), tb_name.size()},
													  std::forward<Args>(args)...);
		}

	  protected:
		const std::string m_tracebuffer;
		const std::string m_msg;
		const std::string m_file;
		const uint32_t m_line;
	};

	struct ErrorTracepoint : public VirtualTracepoint {
		using VirtualTracepoint::VirtualTracepoint;
		~ErrorTracepoint() = default;
		Type type() const noexcept override { return Type::Error; }
		using VirtualTracepoint::make;
	};

} // namespace CommonLowLevelTracingKit::decoder

#endif