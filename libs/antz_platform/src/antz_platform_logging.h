#pragma once

// -------------------------------------------------------------------
// Rationale of antz_platform_logging.h
// -------------------------------------------------------------------
//
// This logger implementation uses an X-macro table to define all log message types in one place,
// specifying their ID, log level, printf-style format string, and argument list. The X-macro approach
// ensures that log message definitions, format strings, and associated helper functions remain consistent
// and easy to maintain, reducing the risk of mismatches.
//
// Key points:
// - All log messages are specified in a single table with their format, level, and typed arguments.
// - The macro generates:
//     - An enum for log message IDs.
//     - Tables for format strings and log levels, used at runtime.
//     - Inline wrapper functions for each log message type to enforce type safety.
//     - A main logger macro (ANTZ_LOGF) that checks the log level at compile time (if constexpr).
// - This scheme allows you to add or change log messages in just one place and ensures all functions
//   and data stay in sync.
// - The implementation is header-only for simplicity and efficiency; all logger helpers are inline.
// - The main logger macro can be enabled or disabled at compile time using the ANTZ_LOG_ENABLE macro.
// - The design avoids problematic macro expansion errors by generating only simple, type-safe
//   wrapper functions per log message.
//
// Benefits:
// - Centralized maintenance: change/add messages in one place
// - Type safety: wrapper functions enforce consistent argument types
// - Efficiency: uses compile-time constants for log levels and format strings
// - Flexibility: log output can be filtered by minimum level using ANTZ_LOG_MIN_LEVEL
// - Easy to extend: add new log messages to the X-macro table as needed
// - Platform agnostic: logger backend can be adapted to resource-constrained embedded systems
//
// -------------------------------------------------------------------
// Adding log messages (for documentation)
// -------------------------------------------------------------------
// This will LOG_FOO_EVENT(int bar, const char* baz)
//
//      #define ANTZ_PLATFORM_LOG_MSGS(X)                   \
//          ... existing log messages ...               \
//          X(FOO_EVENT, antz::LogLevel::Info,          \
//              "Foo happened with bar=%d and baz=%s",  \
//              int bar, const char* baz)
//
// and calling it with
//
//      LOG_FOO_EVENT(43, 'life')
//
// will log the message:
//
//      [INFO] Foo happened with bar=43 and baz=life
//
// -------------------------------------------------------------------
// Usage example (for documentation)
// -------------------------------------------------------------------
//
//  When adding (using)
//
//     LOG_BROADCAST_RAW(channel, msg_id, ext_info, length, hex_str);
//
//  the compiler will it expand to:
//
//     ANTZ_LOGF(antz_platform_log_msg_e::BROADCAST_RAW, channel, msg_id, ext_info, length, hex_str);
//
//  and will only compile this code for levels >= ANTZ_LOG_MIN_LEVEL.
//

#include "logger/antz_logger.h"

#ifndef ANTZ_LOG_ENABLE
#define ANTZ_LOG_ENABLE 1
#endif

#ifndef ANTZ_LOG_MIN_LEVEL
#define ANTZ_LOG_MIN_LEVEL antz::LogLevel::Info
#endif

// -------------------------------------------------------------------
// 1. X Macro Table: id, level, format, (typed_args), (call_args)
// -------------------------------------------------------------------
#define ANTZ_PLATFORM_LOG_MSGS(X)                                                                                   \
    X(BROADCAST_RAW, antz::LogLevel::Fine,                                                                      \
        "[RAW] Channel: %d | Message ID: 0x%02X | %s | Raw Payload (%d): %s",                                   \
        (size_t channel, size_t msg_id, const char* ext_info, size_t length, const char* hex),                  \
        (channel, msg_id, ext_info, length, hex))                                                               \

// -------------------------------------------------------------------
// 2. Log Message ID Enum
// -------------------------------------------------------------------
enum class antz_platform_log_msg_e {
#define X(id, level, fmt, types, values) id,
    ANTZ_PLATFORM_LOG_MSGS(X)
#undef X
    COUNT
};

// -------------------------------------------------------------------
// 3. Log Message Format String Table
// -------------------------------------------------------------------
inline constexpr const char* antz_platform_log_formats[] = {
#define X(id, level, fmt, types, values) fmt,
    ANTZ_PLATFORM_LOG_MSGS(X)
#undef X
};

// -------------------------------------------------------------------
// 4. Log Message Log Level Table
// -------------------------------------------------------------------
inline constexpr antz::LogLevel antz_platform_log_levels[] = {
#define X(id, level, fmt, types, values) level,
    ANTZ_PLATFORM_LOG_MSGS(X)
#undef X
};

// -------------------------------------------------------------------
// 5. Main Logging macro
// -------------------------------------------------------------------
#if ANTZ_LOG_ENABLE
#define ANTZ_LOGF(id, ...)                                                                                      \
    do {                                                                                                        \
        if constexpr (antz_platform_log_levels[int(id)] >= ANTZ_LOG_MIN_LEVEL) {                                    \
            antz::logf(antz_platform_log_levels[int(id)], int(id), antz_platform_log_formats[int(id)], __VA_ARGS__);    \
        }                                                                                                       \
    } while(0)
#else
#define ANTZ_LOGF(id, ...) do { } while(0)
#endif

// -------------------------------------------------------------------
// 6. Generate log functions (typed + value args)
// -------------------------------------------------------------------
#define ANTZ_GEN_LOG_FN(id, level, fmt, types, values)                                                          \
    inline void LOG_##id types {                                                                                \
        ANTZ_LOGF(antz_platform_log_msg_e::id, values);                                                             \
    }
ANTZ_PLATFORM_LOG_MSGS(ANTZ_GEN_LOG_FN)
#undef ANTZ_GEN_LOG_FN