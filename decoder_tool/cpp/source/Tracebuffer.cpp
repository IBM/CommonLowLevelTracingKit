#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"

#include <array>
#include <boost/sort/block_indirect_sort/block_indirect_sort.hpp>
#include <boost/sort/common/range.hpp>
#include <boost/sort/common/util/merge.hpp>
#include <fstream>
#include <optional>
#include <span>
#include <stdint.h>
#include <utility>

#include "CommonLowLevelTracingKit/decoder/Common.hpp"
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "TracepointInternal.hpp"
#include "archive.hpp"
#include "definition.hpp"
#include "file.hpp"
#include "ringbuffer.hpp"
#include "tracebufferfile.hpp"

using Archive = CommonLowLevelTracingKit::decoder::source::low_level::Archive;
static constexpr std::string_view little_endian_magic{"?#$~tracebuffer", 16};
static constexpr std::string_view big_endian_magic{"cart~$#?\0reffube", 16};
namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::exception;
using namespace source;

// Convert internal DefinitionSourceType to public SourceType
static SourceType toPublicSourceType(DefinitionSourceType internal) {
	switch (internal) {
	case DefinitionSourceType::Unknown: return SourceType::Unknown;
	case DefinitionSourceType::Userspace: return SourceType::Userspace;
	case DefinitionSourceType::Kernel: return SourceType::Kernel;
	case DefinitionSourceType::TTY: return SourceType::TTY;
	}
	return SourceType::Unknown;
}

// Determine source type from definition (V2) or file extension (V1 fallback)
static SourceType determineSourceType(const fs::path &path, const Definition &def) {
	SourceType src = toPublicSourceType(def.sourceType());
	if (src == SourceType::Unknown) {
		// V1 fallback: use file extension
		if (path.extension() == ".clltk_ktrace") {
			// Check if it's TTY by name
			if (def.name() == "TTY") {
				src = SourceType::TTY;
			} else {
				src = SourceType::Kernel;
			}
		} else if (path.extension() == ".clltk_trace") {
			src = SourceType::Userspace;
		}
	}
	return src;
}

class SyncTbInternal : public SyncTracebuffer {
  public:
	SyncTbInternal(const fs::path &path)
		: SyncTracebuffer(path, determineSourceType(path, TracebufferFile(path).getDefinition()))
		, m_tracebuffer_file(path)
		, m_file(m_tracebuffer_file.getFilePart())
		, m_file_size(m_file.getFileSize()) {
		if (size() < 3) CLLTK_DECODER_THROW(InvalidTracebuffer, "ringbuffer too small");
	}
	~SyncTbInternal() override {}
	const std::string_view name() const noexcept override;
	size_t size() const noexcept override;

	TracepointPtr next(const TracepointFilterFunc &filter) noexcept override;
	TracepointPtr next_pooled(TracepointPool &pool,
							  const TracepointFilterFunc &filter) noexcept override;
	uint64_t pending() noexcept override;
	uint64_t current_top_entries_nr() const noexcept override;

  private:
	TracebufferFile m_tracebuffer_file;
	const FilePart m_file;
	size_t m_file_size;
};
SyncTracebufferPtr SyncTracebuffer::make(const fs::path &path) {
	if (is_tracebuffer(path)) try {
			return std::make_unique<SyncTbInternal>(path);
		} catch (...) {}
	return nullptr;
}

const std::string_view SyncTbInternal::name() const noexcept {
	return m_tracebuffer_file.getDefinition().name();
}
size_t SyncTbInternal::size() const noexcept {
	return m_tracebuffer_file.getRingbuffer().getSize();
}
uint64_t SyncTbInternal::current_top_entries_nr() const noexcept {
	return m_tracebuffer_file.getRingbuffer().getEntryCount();
};

TracepointPtr SyncTbInternal::next(const TracepointFilterFunc &filter) noexcept {
	while (true) {
		auto ringbuffer_entry = m_tracebuffer_file.getRingbuffer().getNextEntry();
		if (auto s = std::get_if<std::string>(&ringbuffer_entry))
			return ErrorTracepoint::make(name(), *s);
		auto &e = std::get<Ringbuffer::EntryPtr>(ringbuffer_entry);
		if (e == nullptr) return {};

		const uint64_t fileoffset = get<uint64_t>(e->body()) & ((1ULL << 48) - 1);
		if (fileoffset == 0x01) {
			auto tp = make_tracepoint<TracepointDynamic>(name(), std::move(e), m_source_type);
			if (!filter || filter(*tp)) return tp;
		} else if (fileoffset < 0xFF) {
			return ErrorTracepoint::make(
				name(), "invalid file offset: value is less than minimum valid offset (0xFF)");
		} else {
			if (fileoffset > m_file_size) m_file_size = m_file.grow();
			if ((fileoffset + sizeof(uint32_t)) > m_file_size)
				return ErrorTracepoint::make(name(), "file offset bigger than file");
			if (m_file.get<char>(fileoffset) != '{')
				return ErrorTracepoint::make(
					name(), "invalid meta magic at offset " + std::to_string(fileoffset) +
								": expected '{', found '" +
								std::string(1, m_file.get<char>(fileoffset)) + "'");
			const uint32_t meta_size = m_file.get<uint32_t>(fileoffset + 1);
			if (meta_size == 0) return ErrorTracepoint::make(name(), "invalid meta size (0)");
			if ((fileoffset + meta_size) > m_file_size)
				return ErrorTracepoint::make(name(), "meta entry bigger than file end");
			const std::span<const uint8_t> meta{&m_file.getReference<const uint8_t>(fileoffset),
												meta_size};
			auto tp = make_tracepoint<TracepointStatic>(name(), std::move(e), meta,
														m_file.getFileInternal(), m_source_type);
			if (!filter || filter(*tp)) return tp;
		}
	}
}

TracepointPtr SyncTbInternal::next_pooled(TracepointPool &pool,
										  const TracepointFilterFunc &filter) noexcept {
	while (true) {
		auto ringbuffer_entry = m_tracebuffer_file.getRingbuffer().getNextEntry();
		if (auto s = std::get_if<std::string>(&ringbuffer_entry))
			return ErrorTracepoint::make(name(), *s);
		auto &e = std::get<Ringbuffer::EntryPtr>(ringbuffer_entry);
		if (e == nullptr) return {};

		const uint64_t fileoffset = get<uint64_t>(e->body()) & ((1ULL << 48) - 1);
		if (fileoffset == 0x01) {
			auto tp = make_pooled_tracepoint<TracepointDynamic>(pool, name(), std::move(e),
																m_source_type);
			if (!filter || filter(*tp)) return tp;
		} else if (fileoffset < 0xFF) {
			return ErrorTracepoint::make(
				name(), "invalid file offset: value is less than minimum valid offset (0xFF)");
		} else {
			if (fileoffset > m_file_size) m_file_size = m_file.grow();
			if ((fileoffset + sizeof(uint32_t)) > m_file_size)
				return ErrorTracepoint::make(name(), "file offset bigger than file");
			if (m_file.get<char>(fileoffset) != '{')
				return ErrorTracepoint::make(
					name(), "invalid meta magic at offset " + std::to_string(fileoffset) +
								": expected '{', found '" +
								std::string(1, m_file.get<char>(fileoffset)) + "'");
			const uint32_t meta_size = m_file.get<uint32_t>(fileoffset + 1);
			if (meta_size == 0) return ErrorTracepoint::make(name(), "invalid meta size (0)");
			if ((fileoffset + meta_size) > m_file_size)
				return ErrorTracepoint::make(name(), "meta entry bigger than file end");
			const std::span<const uint8_t> meta{&m_file.getReference<const uint8_t>(fileoffset),
												meta_size};
			auto tp = make_pooled_tracepoint<TracepointStatic>(
				pool, name(), std::move(e), meta, m_file.getFileInternal(), m_source_type);
			if (!filter || filter(*tp)) return tp;
		}
	}
}

uint64_t SyncTbInternal::pending() noexcept {
	return m_tracebuffer_file.getRingbuffer().pendingBytes();
}

bool Tracebuffer::is_tracebuffer(const fs::path &path) {
	if (!path.has_extension()) return false;
	const auto extension = path.extension();
	if (extension != ".clltk_trace" && extension != ".clltk_ktrace") return false;

	if (!fs::exists(path)) return false;
	std::ifstream file{path, std::ios::binary};
	if (!file) return false;

	std::array<char, 16> fileHead{};
	file.read(fileHead.begin(), fileHead.size());
	if (safe_cast<size_t>(file.gcount()) < fileHead.size()) return false;

	const std::string_view magic{fileHead.data(), fileHead.size()};
	if (magic == little_endian_magic) return true;
	if (magic == big_endian_magic) return false; // currently not supported
	return false;
}
bool SnapTracebuffer::is_formattable(const fs::path &path) {
	if (is_tracebuffer(path)) return true;
	if (Archive::is_archive(path)) return true;
	return false;
}

SnapTracebuffer::SnapTracebuffer(const std::filesystem::path &path, TracepointCollection &&tps,
								 std::string &&name, size_t size, SourceType source_type) noexcept
	: Tracebuffer(path, source_type)
	, tracepoints(std::move(tps))
	, m_name(std::move(name))
	, m_size(size) {}

SnapTracebufferPtr SnapTracebuffer::make(const fs::path &path,
										 const TracepointFilterFunc &tracepointFilter) {
	auto sync_tb = SyncTracebuffer::make(path);
	if (!sync_tb) return {};
	std::string name{sync_tb->name()};
	auto size = sync_tb->size();
	auto source_type = sync_tb->sourceType();
	TracepointCollection tps{};
	const uint64_t top_nr = sync_tb->current_top_entries_nr();

	while (true) {
		TracepointPtr e = sync_tb->next();
		if (!e || (e->nr > top_nr)) break;
		if (!tracepointFilter || tracepointFilter(*e)) tps.push_back(std::move(e));
	}

	static constexpr auto cmp = [](const auto &a, const auto &b) -> bool {
		return a->timestamp_ns < b->timestamp_ns;
	};
	boost::sort::block_indirect_sort(tps.begin(), tps.end(), cmp);
	SnapTracebufferPtr tb{
		new SnapTracebuffer(path, std::move(tps), std::move(name), size, source_type)};
	return tb;
}
static INLINE void add(SnapTracebufferCollection &out, SnapTracebufferCollection &&in) {
	for (auto &t : in)
		if (t) out.push_back(std::move(t));
}
SnapTracebufferCollection SnapTracebuffer::collect(const fs::path &path,
												   const TracebufferFilterFunc &tracebufferFilter,
												   const TracepointFilterFunc &tracepointFilter) {
	SnapTracebufferCollection out{};
	if (!fs::exists(path)) return out;
	if (fs::is_directory(path)) {
		for (auto const &entry : fs::recursive_directory_iterator(path)) {
			if (Tracebuffer::is_tracebuffer(entry))
				add(out, collect(entry, tracebufferFilter, tracepointFilter));
		}
	} else if (auto archive = Archive::make(path)) {
		add(out, collect(archive->dir(), tracebufferFilter, tracepointFilter));
	} else if (Tracebuffer::is_tracebuffer(path)) {
		auto tp = make(path, tracepointFilter);
		if (tp && (!tracebufferFilter || tracebufferFilter(*tp))) out.push_back(std::move(tp));
	}
	return out;
}
