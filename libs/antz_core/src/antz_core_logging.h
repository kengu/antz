#pragma once

#include <functional>
#include <logger/antz_logger.h>

//std::function<std::string()>
//std::function<const char*>

// -------------------------------------------------------------------
// Define module specific logging methods
// -------------------------------------------------------------------
#define ANTZ_CORE_LOG_MSGS(X)                                                                                   \
    X(ANTZ_CORE, BROADCAST_RAW, antz::LogLevel::Fine,                                                           \
        "[RAW] Channel: %d | Message ID: 0x%02X | %s | Raw Payload (%d): %s",                                   \
        (size_t channel, size_t msg_id, const char* ext_info, size_t length, const char* hex), \
        channel, msg_id, ext_info, length, hex                                                                  \
    )                                                                                                           \
    X(ANTZ_CORE, NO_EXT_INFO, antz::LogLevel::Warn,                                                             \
        "[parse_ext_fields] No Extended Info, message length is < 10", ()                                       \
    )                                                                                                           \
    X(ANTZ_CORE, EXT_INFO_LENGTH_EXCEEDED_MESSAGE_LENGTH, antz::LogLevel::Warn,                                 \
        "[parse_ext_fields] Extended Info length exceeds Message length!", ()                                   \
    )

#include "logger/antz_xmacro_logger.h"
ANTZ_XMACRO_LOGS(ANTZ_CORE, ANTZ_CORE_LOG_MSGS)