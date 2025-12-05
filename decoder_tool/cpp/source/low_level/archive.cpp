#include "archive.hpp"
#include "inline.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>
#include <stddef.h>
#include <string>

using namespace CommonLowLevelTracingKit::decoder::source::low_level;
namespace fs = std::filesystem;
static INLINE fs::path get_unique_tmp_dir_path() {
	static const fs::path tmp_base = fs::temp_directory_path();
	static boost::uuids::random_generator gen;
	boost::uuids::uuid id = gen();
	const fs::path tmp = tmp_base / boost::uuids::to_string(id);
	fs::create_directories(tmp);
	return tmp;
}
bool Archive::is_archive(const fs::path &path) {
	if (!fs::is_regular_file(path)) return false;
	struct ::archive *a = archive_read_new();
	archive_read_support_format_all(a); // Enable all format support
	archive_read_support_filter_all(a); // Enable all decompression support

	if (archive_read_open_filename(a, path.c_str(), 10240) != ARCHIVE_OK) {
		archive_read_free(a);
		return false;
	}
	[[maybe_unused]] const int archive_type = archive_format(a);
	archive_read_close(a);
	archive_read_free(a);

	return true;
}

ArchivePtr Archive::make(const fs::path &path) {
	if (!is_archive(path)) return {};
	return ArchivePtr{new Archive(path)};
}
Archive::Archive(const fs::path &path)
	: m_archive(path)
	, m_tmp(get_unique_tmp_dir_path())
	, a(archive_read_new()) {
	unpack();
}

Archive::~Archive() {
	if (fs::is_directory(m_tmp)) { fs::remove_all(m_tmp); }
	if (a != nullptr) {
		archive_read_free(a);
		a = nullptr;
	}
}
void Archive::unpack() {
	archive_read_support_format_all(a);
	archive_read_support_filter_all(a);

	if (archive_read_open_filename(a, m_archive.c_str(), 10240) != ARCHIVE_OK) { return; }

	struct archive_entry *entry;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		const char *pathname = archive_entry_pathname(entry);
		if (pathname == nullptr) {
			archive_read_data_skip(a);
			continue;
		}
		fs::path dest = m_tmp / pathname;

		if (archive_entry_filetype(entry) == AE_IFDIR) {
			fs::create_directories(dest);
		} else {
			fs::create_directories(dest.parent_path());
			std::ofstream ofs(dest, std::ios::binary);
			if (!ofs) {
				archive_read_data_skip(a);
				continue;
			}
			const void *buffer;
			size_t size;
			la_int64_t offset;
			while (archive_read_data_block(a, &buffer, &size, &offset) == ARCHIVE_OK) {
				ofs.write(static_cast<const char *>(buffer), safe_cast<long>(size));
			}
		}
		archive_read_data_skip(a);
	}

	archive_read_close(a);
	return;
}