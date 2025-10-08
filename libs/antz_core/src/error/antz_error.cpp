#include "antz_error.h"
#include <mutex>

namespace antz
{

    // Static error object and mutex for thread safety
    static Error last_error;
    static std::mutex error_mutex;

    void set_error(const Error& code)
    {
        std::lock_guard<std::mutex> lock(error_mutex);
        last_error = code;
    }

    void set_error(const int code,
                   const std::string& source,
                   const std::string& message,
                   const std::string& file,
                   const int line)
    {
        set_error(Error(code, source, message, file, line));
    }

    void log_error(const Error& error, const antz::LogLevel level) {
        // Compose a log message with error details
        logf(level, error.code,
             "Error (%d) in %s: %s [file: %s, line: %s]",
             error.code,
             error.source.c_str(),
             error.message.c_str(),
             error.file.empty() ? "<none>" : error.file.c_str(),
             error.line == 0 ? "<none>" : std::to_string(error.line).c_str()
        );
    }


} // namespace antz
