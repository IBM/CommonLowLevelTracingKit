#ifndef DECODER_TOOL_SOURCE_TRACEPOINT_INTERNAL_HEADER
#define DECODER_TOOL_SOURCE_TRACEPOINT_INTERNAL_HEADER
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "file.hpp"
#include "inline.hpp"
#include "ringbuffer.hpp"
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

	struct TraceEntryHead : public Tracepoint {
		const uint32_t m_pid;
		const uint32_t m_tid;
		TraceEntryHead(std::string_view tb_name, uint64_t n, uint64_t t,
					   const std::span<const uint8_t> &body);
		TraceEntryHead(std::string_view tb_name, uint64_t n, uint64_t t, uint32_t pid,
					   uint32_t tid);
		uint32_t pid() const noexcept override { return m_pid; };
		uint32_t tid() const noexcept override { return m_tid; };
	};

	class TracepointDynamic final : public TraceEntryHead {
	  public:
		~TracepointDynamic() = default;
		TracepointDynamic(const std::string_view tb_name, source::Ringbuffer::EntryPtr entry);

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
						 const std::span<const uint8_t> &meta, const source::internal::FilePtr &&);

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
		VirtualTracepoint(const std::string tb_name, const std::string &msg)
			: TraceEntryHead(tb_name, 0, 0, 0, 0)
			, m_tracebuffer(std::move(tb_name))
			, m_msg(msg)
			, m_file()
			, m_line() {}
		VirtualTracepoint(const std::string tb_name, const source::Ringbuffer::Entry &e,
						  const std::string &msg)
			: TraceEntryHead(tb_name, e.nr, get<uint64_t>(e.body(), 14), 0, 0)
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
		static INLINE constexpr auto make(const std::string_view tb_name, Args &&...args) {
			return std::make_unique<VirtualTracepoint>(std::string{tb_name.data(), tb_name.size()},
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