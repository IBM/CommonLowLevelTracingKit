#include "CommonLowLevelTracingKit/Decoder/Tracebuffer.hpp"

#include <array>
#include <boost/sort/block_indirect_sort/block_indirect_sort.hpp>
#include <boost/sort/common/range.hpp>
#include <boost/sort/common/util/merge.hpp>
#include <fstream>
#include <span>
#include <stdint.h>
#include <utility>

#include "CommonLowLevelTracingKit/Decoder/Common.hpp"
#include "CommonLowLevelTracingKit/Decoder/Tracepoint.hpp"
#include "TracepointInternal.hpp"
#include "archive.hpp"
#include "definition.hpp"
#include "file.hpp"
#include "ringbuffer.hpp"
#include "tracebufferfile.hpp"

using Archive = CommonLowLevelTracingKit::decoder::source::low_level::Archive;
static constexpr std::string_view little_ending_magic{"?#$~tracebuffer", 16};
static constexpr std::string_view big_ending_maci{"cart~$#?\0reffube", 16};
namespace fs = std::filesystem;
using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::exception;
using namespace source;
class SyncTbInternal : public SyncTracebuffer {
  public:
	SyncTbInternal(const fs::path &a_path)
		: SyncTracebuffer(a_path)
		, m_tb(a_path)
		, m_file(m_tb.getFilePart())
		, m_file_size(m_file.getFileSize()) {}
	~SyncTbInternal() override {}
	const std::string_view name() const noexcept override;
	size_t size() const noexcept override;

	TracepointPtr next(const TracepointFilterFunc &filter) override;
	uint64_t pending() noexcept override;
	uint64_t current_top_entries_nr() const noexcept override;

  private:
	TracebufferFile m_tb;
	const FilePart m_file;
	size_t m_file_size;
	const TracepointFilterFunc m_filter;
};
SyncTracebufferPtr SyncTracebuffer::make(const fs::path &path) {
	if (is_tracebuffer(path)) try {
			return std::make_unique<SyncTbInternal>(path);
		} catch (...) {}
	return nullptr;
}

const std::string_view SyncTbInternal::name() const noexcept {
	return m_tb.getDefinition().name();
}
size_t SyncTbInternal::size() const noexcept {
	return m_tb.getRingbuffer().getSize();
}
uint64_t SyncTbInternal::current_top_entries_nr() const noexcept {
	return m_tb.getRingbuffer().getEntrieCount();
};

TracepointPtr SyncTbInternal::next(const TracepointFilterFunc &filter) {
	while (true) {

		auto e = m_tb.getRingbuffer().getNextEntry();
		if (e == nullptr) return {};

		const uint64_t fileoffset = get<uint64_t>(e->body()) & ((1ULL << 48) - 1);
		if (fileoffset == 0x01) {
			auto tp = std::make_unique<TracepointDynamic>(name(), std::move(e));
			if (!filter || filter(*tp)) return tp;
		} else if (fileoffset < 0xFF) {
			CLLTK_DECODER_THROW(InvalidEntry, "invalid file offset");
		} else {
			if (fileoffset > m_file_size) m_file_size = m_file.doGrow();
			if (fileoffset > m_file_size)
				CLLTK_DECODER_THROW(InvalidEntry, "file offset bigger than file");
			if (m_file.get<char>(fileoffset) != '{')
				CLLTK_DECODER_THROW(InvalidMeta, "invalid meta magic");
			const uint32_t meta_size = m_file.get<uint32_t>(fileoffset + 1);
			const std::span<const uint8_t> meta{&m_file.getRef<const uint8_t>(fileoffset),
												meta_size};
			auto tp = std::make_unique<TracepointStatic>(name(), std::move(e), meta,
														 m_file.getFileInternal());
			if (!filter || filter(*tp)) return tp;
		}
	}
}

uint64_t SyncTbInternal::pending() noexcept {
	return m_tb.getRingbuffer().pendingBytes();
}

bool Tracebuffer::is_tracebuffer(const fs::path &path) {
	if (path.has_extension()) {
		const auto extension = path.extension();
		if (extension != ".clltk_trace" && path.extension() != ".clltk_ktrace") return false;
	}

	if (!fs::exists(path)) return false;
	std::ifstream file{path, std::ios::binary};

	if (!file) { return false; }
	std::array<char, 16> fileHead{};
	file.read(fileHead.begin(), fileHead.size());
	if (safe_narrow_cast<size_t>(file.gcount()) < fileHead.size()) { return false; }

	const std::string_view magic{fileHead.data(), fileHead.size()};
	if (magic == little_ending_magic) return true;
	if (magic == big_ending_maci) return false; // currently not supported
	return false;
}
bool SnapTracebuffer::is_formattable(const fs::path &path) {
	if (is_tracebuffer(path)) return true;
	if (Archive::is_archive(path)) return true;
	return false;
}

SnapTracebuffer::SnapTracebuffer(const std::filesystem::path &path, TracepointCollection &&tps,
								 std::string &&name, size_t size) noexcept
	: Tracebuffer(path)
	, tracepoints(std::move(tps))
	, m_name(std::move(name))
	, m_size(size) {}

SnapTracebufferPtr SnapTracebuffer::make(const fs::path &path, TracepointFilterFunc tpFilter) {
	auto sync_tb = SyncTracebuffer::make(path);
	if (!sync_tb) return {};
	std::string name{sync_tb->name()};
	auto size = sync_tb->size();
	TracepointCollection tps{};
	const uint64_t top_nr = sync_tb->current_top_entries_nr();

	while (true) {
		try {
			TracepointPtr e = sync_tb->next();
			if (!e || (e->nr > top_nr)) break;
			if (!tpFilter || tpFilter(*e)) tps.push_back(std::move(e));
		} catch (CommonLowLevelTracingKit::decoder::exception::Exception &) {}
	}

	static constexpr auto cmp = [](const auto &a, const auto &b) -> bool {
		return a->timestamp_ns < b->timestamp_ns;
	};
	boost::sort::block_indirect_sort(tps.begin(), tps.end(), cmp);
	SnapTracebufferPtr tb{new SnapTracebuffer(path, std::move(tps), std::move(name), size)};
	return tb;
}
static INLINE void add(SnapTracebufferCollection &out, SnapTracebufferCollection &&in) {
	for (auto &t : in)
		if (t) out.push_back(std::move(t));
}
SnapTracebufferCollection SnapTracebuffer::collect(const fs::path &path,
												   TracebufferFilterFunc tbFilter,
												   TracepointFilterFunc tpFilter) {
	SnapTracebufferCollection out{};
	if (!fs::exists(path)) return out;
	if (fs::is_directory(path)) {
		for (auto const &entry : fs::recursive_directory_iterator(path)) {
			if (Tracebuffer::is_tracebuffer(entry)) add(out, collect(entry, tbFilter, tpFilter));
		}
	} else if (auto archive = Archive::make(path)) {
		add(out, collect(archive->dir(), tbFilter, tpFilter));
	} else if (Tracebuffer::is_tracebuffer(path)) {
		auto tp = make(path, tpFilter);
		if (tp && (!tbFilter || tbFilter(*tp))) out.push_back(std::move(tp));
	}
	return out;
}
