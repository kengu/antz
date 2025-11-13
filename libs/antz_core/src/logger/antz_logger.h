//
// Created by Kenneth Gulbrandsøy on 19/05/2025.
//

#pragma once

#include <string>
#include <cstdint>
#include <cstdarg>

namespace antz {

    enum class LogLevel {
        Fine,
        Info,
        Warn,
        Panic
    };

    constexpr size_t MAX_LOG_BUFFER = 256;

    /**
     * @brief Provides a thread-local temporary character buffer of fixed size MAX_LOG_BUFFER
     *
     * This function returns a pointer to a buffer intended for temporary, non-persistent use,
     * such as formatting log messages or performing short-lived string manipulations.
     * The buffer is unique per thread due to `thread_local` storage, ensuring thread safety
     * without dynamic memory allocation.
     *
     * @param[out] out_size Optional pointer to a variable that will be set to the buffer size.
     *                      Pass `nullptr` if you do not need the buffer size.
     * @return char*        Pointer to the thread-local buffer.
     *
     * @note Each call from the same thread returns the same buffer pointer.
     *       Contents may be overwritten by later calls; copy the data if it needs to be preserved.
     *       Do not use the buffer after the thread has exited.
     *
     * Example of usage @code
     *   size_t size;
     *   char *buf = temp_log_buffer(&size);
     *   snprintf(buf, size, "Error code: %d", get_error_code());
     * @endcode
     */
    inline char* temp_log_buffer(size_t* out_size = nullptr) {
        thread_local char buf[MAX_LOG_BUFFER];
        if (out_size) *out_size = MAX_LOG_BUFFER;
        return buf;
    }

    class Logger {
        public:
            virtual ~Logger() = default;
            virtual void logf(LogLevel level, int msg_id, const char* fmt, va_list ap) = 0;
    };

    extern Logger* g_logger;

    const char* formatf(const char* fmt, ...);
    void logf(LogLevel level, int msg_id, const char* fmt, ...);

    inline void finef(const int msg_id, const char* fmt, ...){
        logf(LogLevel::Fine, msg_id, fmt);
    }

    inline void infof(const int msg_id, const char* fmt, ...){
        logf(LogLevel::Info, msg_id, fmt);
    }

    inline void warnf(const int msg_id, const char* fmt, ...){
        logf(LogLevel::Warn, msg_id, fmt);
    }

    inline void panicf(const int msg_id, const char* fmt, ...){
        logf(LogLevel::Panic, msg_id, fmt);
    }

    // Single byte (uint8_t) as hex into temp_log_buffer
    inline const char* to_hex_byte(const uint8_t byte) {
        size_t size;
        char* buf = temp_log_buffer(&size);
        snprintf(buf, size, "%02X", byte);
        return buf;
    }

    // Single word (uint16_t) as hex into temp_log_buffer
    inline const char* to_hex_byte(const uint16_t word) {
        size_t size;
        char* buf = temp_log_buffer(&size);
        snprintf(buf, size, "%04X", word);
        return buf;
    }

    // Hex dump into temp_log_buffer (uint8_t version)
    inline const char* to_hex(const uint8_t* d, const uint8_t length) {
        size_t size;
        char* buf = temp_log_buffer(&size);
        size_t pos = 0;
        for (uint8_t i = 0; i < length && pos + 3 < size; ++i) {
            pos += snprintf(buf + pos, size - pos, "%02X ", d[i]);
        }
        // Strip trailing space if needed
        if (pos > 0 && buf[pos - 1] == ' ') buf[pos - 1] = '\0';
        return buf;
    }

    // Hex dump into temp_log_buffer (uint16_t version)
    inline const char* to_hex(const uint16_t* d, const uint8_t length) {
        size_t size;
        char* buf = temp_log_buffer(&size);
        size_t pos = 0;
        for (uint8_t i = 0; i < length && pos + 5 < size; ++i) {
            pos += snprintf(buf + pos, size - pos, "%04X ", d[i]);
        }
        if (pos > 0 && buf[pos - 1] == ' ') buf[pos - 1] = '\0';
        return buf;
    }

} // namespace antz