//
// Created by Kenneth Gulbrandsøy on 19/05/2025.
//

#include "antz_logger.h"
#include <cstdio>
#include <cstdarg>

namespace antz {

    class StderrLogger final : public Logger {
    public:
        // Implementation of logf
        void logf(const LogLevel level, int /*msg_id*/, const char* fmt, const va_list ap) override {
            auto level_str = "";
            switch (level) {
                case LogLevel::Fine:    level_str = "[FINE] "; break;
                case LogLevel::Info:    level_str = "[INFO] "; break;
                case LogLevel::Warn:    level_str = "[WARN] "; break;
                case LogLevel::Panic:   level_str = "[PANIC] "; break;
            }
            fprintf(stderr, "%s", level_str);
            vfprintf(stderr, fmt, ap);
            fputc('\n', stderr);
        }
    };

    StderrLogger default_logger;

    const char* formatf(const char* fmt, ...) {
        char* buffer = temp_log_buffer();
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buffer, MAX_LOG_BUFFER, fmt, ap);
        va_end(ap);
        return buffer;
    }

    void logf(const LogLevel level, const int msg_id, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        if (g_logger)
            g_logger->logf(level, msg_id, fmt, ap);
        va_end(ap);
    }

} // namespace antz