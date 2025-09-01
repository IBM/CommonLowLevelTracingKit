#pragma once

#include <filesystem>
#include <memory>

struct archive;
namespace CommonLowLevelTracingKit::decoder::source::low_level {
	struct Archive;
	using ArchivePtr = std::unique_ptr<Archive>;
	class Archive {
	  public:
		static bool is_archive(const std::filesystem::path &);
		static ArchivePtr make(const std::filesystem::path &);
		virtual ~Archive();
		const std::filesystem::path &dir() const { return m_tmp; }

	  protected:
		void unpack();
		Archive(const std::filesystem::path &);
		const std::filesystem::path m_archive;
		const std::filesystem::path m_tmp;
		struct ::archive *a;
	};
} // namespace CommonLowLevelTracingKit::decoder::source::low_level
