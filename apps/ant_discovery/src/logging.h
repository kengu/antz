//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//
#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

namespace ant {
    enum class LogLevel {
        Fine,
        Info,
        Warn,
        Error,
        None = 256,
    };

    static auto logLevel = LogLevel::Info;

    inline std::string currentTimestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        std::time_t now_c = system_clock::to_time_t(now);
        std::tm* parts = std::localtime(&now_c);

        std::ostringstream oss;
        oss << std::put_time(parts, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    inline void log(const LogLevel level, const std::string& message) {
        if (level < logLevel) return;

        auto name = "";
        switch (level) {
        case LogLevel::Fine:  name = "[FINE]"; break;
        case LogLevel::Info:  name = "[INFO]"; break;
        case LogLevel::Warn:  name = "[WARN]"; break;
        case LogLevel::Error: name = "[ERROR]"; break;
        case LogLevel::None: name = "[NONE]"; break;
        }

        const auto now = std::chrono::system_clock::now();
        const std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        const std::tm* tm_ptr = std::localtime(&now_c);
        char timeBuf[20];
        std::strftime(timeBuf, sizeof(timeBuf), "%F %T", tm_ptr);

        std::cout << timeBuf << ": " << name << ": " << message << std::endl;
    }

    inline void fine(const std::string& message) {
        log(LogLevel::Fine, message);
    }

    inline void info(const std::string& message) {
        log(LogLevel::Info, message);
    }

    inline void warn(const std::string& message) {
        log(LogLevel::Warn, message);
    }

    inline void error(const std::string& message) {
        log(LogLevel::Error, message);
    }

    // Allow choosing verbosity of logging
    inline void setLogLevel(const LogLevel level)  {
        logLevel = level;
        fine("Setting log level to [" + std::to_string(static_cast<int>(level)) + "]");
    }

    inline std::string toHex(const uint8_t* d, int length) {
        std::ostringstream oss;
        for (int i = 0; i < length; ++i) {
            oss << std::uppercase << std::hex
                << std::setw(2) << std::setfill('0')
                << static_cast<int>(d[i]) << " ";
        }
        return oss.str();
    }

    inline std::string toHexByte(const uint8_t byte) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        return oss.str();
    }

    inline std::string toHexByte(const uint16_t byte) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(byte);
        return oss.str();
    }

}

#endif // LOGGING_H

