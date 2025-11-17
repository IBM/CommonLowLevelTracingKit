#ifndef DECODER_TOOL_COMMON_HEADER
#define DECODER_TOOL_COMMON_HEADER

#if defined(__GNUC__) || defined(__clang__)
	#define EXPORT __attribute__((visibility("default")))
#else
	#define EXPORT
#endif

#include <exception>
#include <string>

namespace CommonLowLevelTracingKit::decoder::exception {
	struct Exception : public std::exception {
		Exception(const std::string &msg, const char *file, unsigned long line)
			: message(msg + " at " + file + ":" + std::to_string(line)) {}
		const char *what() const noexcept override { return message.c_str(); }

	  private:
		std::string message;
	};
	struct FormattingFailed : public Exception {
		FormattingFailed(const std::string &msg, const char *file, unsigned long line)
			: Exception("FormattingFailed: " + msg, file, line) {}
	};
	struct InvalidEntry : public Exception {
		InvalidEntry(const std::string &msg, const char *file, unsigned long line)
			: Exception("InvalidEntry: " + msg, file, line) {}
	};
	struct InvalidMeta : public Exception {
		InvalidMeta(const std::string &msg, const char *file, unsigned long line)
			: Exception("InvalidMeta: " + msg, file, line) {}
	};
	struct Synchronisation : public Exception {
		Synchronisation(const std::string &msg, const char *file, unsigned long line)
			: Exception("Synchronisation: " + msg, file, line) {}
	};
	struct InvalidTracebuffer : public Exception {
		InvalidTracebuffer(const std::string &msg, const char *file, unsigned long line)
			: Exception("InvalidTracebuffer: " + msg, file, line) {}
	};
#define CLLTK_DECODER_THROW(TYPE, MSG) throw TYPE(MSG, __FILE__, __LINE__)

} // namespace CommonLowLevelTracingKit::decoder::exception
#endif