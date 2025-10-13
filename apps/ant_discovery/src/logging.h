//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//
#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

enum class LogLevel {
    INFO,
    FINE,
    SEVERE
};

inline std::string currentTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t now_c = system_clock::to_time_t(now);
    std::tm* parts = std::localtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(parts, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

inline void log(const LogLevel level, const std::string& msg) {
    const char* tag = nullptr;
    switch (level) {
        case LogLevel::INFO:   tag = "[INFO]"; break;
        case LogLevel::FINE:   tag = "[FINE]"; break;
        case LogLevel::SEVERE: tag = "[SEVERE]"; break;
    }
    std::cout << currentTimestamp() << ": " << tag << ": " << msg << std::endl;
}

inline void info(const std::string& msg)   { log(LogLevel::INFO, msg);  }
inline void fine(const std::string& msg)   { log(LogLevel::FINE, msg);  }
inline void severe(const std::string& msg) { log(LogLevel::SEVERE, msg); }

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

#endif // LOGGING_H

