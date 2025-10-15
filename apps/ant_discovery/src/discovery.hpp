#pragma once

#include "types.h"
#include <usb_device_handle.hpp>

namespace ant {

    enum class LogLevel {
        Fine,
        Info,
        Warn,
        Error,
        None = 256,
    };

    // ===== Output format support (Text / JSON / CSV) =====
    enum class OutputFormat {
        Text,
        JSON,
        CSV
    };

    void setFormat(OutputFormat fmt);
    void setLogLevel(LogLevel level);
    void setEpsLatLng(double meters);
    void setEpsHeading(double degrees);
    void setMqtt(const std::string& cnn);
    bool initialize(const USBDevice& pDevice, UCHAR ucDeviceNumber);
    bool startDiscovery();
    void runEventLoop();
    void cleanup();
}
