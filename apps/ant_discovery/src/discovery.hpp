#pragma once


#include <string>
#include <vector>
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

    enum class AntProfile {
        Unknown,
        HeartRate,
        AssetTracker,
    };

    enum class OutputFormat {
        Text,
        JSON,
        CSV
    };


    void setFormat(OutputFormat fmt);
    void setLogLevel(LogLevel level);
    void setSearch(const std::vector<AntProfile>& types);
    void setEpsLatLng(double meters);
    void setEpsHeading(double degrees);
    void setMqtt(const std::string& cnn);
    bool initialize(ULONG baud, UCHAR ucDeviceNumber);
    bool startDiscovery();
    void runEventLoop();
    void cleanup();
}
