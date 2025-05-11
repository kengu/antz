//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//

// ant_device.h
#ifndef ANT_DEVICE_H
#define ANT_DEVICE_H

#include <iomanip>
#include <sstream>
#include <string>

// Structure to hold information about an asset or device.
struct Device {
    uint8_t index{};
    std::string fullName;
    uint8_t icon = 0;
    uint16_t distance = 0;
    float headingDegrees = 0.0f;
    double latitude = 0.0;
    double longitude = 0.0;
    bool gpsLost = false;
    bool commsLost = false;
    bool remove = false;
    bool lowBattery = false;
    std::string batteryLevel = "Unknown";
    uint8_t colour = 0;
    uint8_t type = 0;
};

// Structure for extended info, such as RSSI, proximity, etc.
struct ExtendedInfo {

    bool hasDChID = false;
    bool hasRssi = false;
    bool hasProximity = false;
    bool hasTType = false;
    bool hasDType = false;

    uint16_t number = 0;
    uint8_t dType = 0;
    uint8_t tType = 0;

    // The measurement value represents the measurement type of
    // the received data message, and indicates how to interpret
    // the RSSI Value field. The measurement type will be 0x20,
    // which refers to DBM type. DBM type indicates that the RSSI
    // value is taken in units of dBm. If the measurement type is
    // any other value, do not decode any other bytes in the
    // extended RSSI data. For more information refer to the
    // “RSSI Extended Messaging” application note.
    uint8_t mType = 0;

    // The RSSI value is a signed integer that corresponds
    // to the measured RSSI value in dBm (can be negative).
    int8_t rssi = 0;

    // The threshold configuration value is used to indicate
    // the dBm value of the bin configured using the
    // Proximity Search command. The default value is -128dB,
    //  which corresponds to an effective “ Off” setting.
    uint8_t threshold = 0;   // Proximity (0–255)

    // Length of extended info
    uint8_t length = 0;
};

inline ExtendedInfo parseExtendedInfo(const uint8_t* trailerStart, const uint8_t flags) {
    ExtendedInfo info;
    size_t offset = 0;

    if (flags & (1 << 4)) { // Proximity
        info.threshold = trailerStart[offset++];
        info.hasProximity = true;
    }

    if (flags & (1 << 3)) { // RSSI
        info.rssi = static_cast<int8_t>(trailerStart[offset++]); // signed!
        info.hasRssi = true;
    }

    if (flags & (1 << 2)) { // Channel Type (skipped)
        offset++; // ignore or capture if needed
    }

    if (flags & (1 << 1)) { // TxType
        info.tType = trailerStart[offset++];
        info.hasTType = true;
    }

    if (flags & (1 << 0)) { // DevType
        info.dType = trailerStart[offset++];
        info.hasDType = true;
    }

    info.length = static_cast<uint8_t>(offset);
    return info;
}

inline uint8_t trailerLengthGuess(uint8_t flags) {
    uint8_t len = 0;
    if (flags & (1 << 4)) len++; // Proximity
    if (flags & (1 << 3)) len++; // RSSI
    if (flags & (1 << 2)) len++; // Channel Type
    if (flags & (1 << 1)) len++; // TxType
    if (flags & (1 << 0)) len++; // DevType
    return len;
}

inline std::string describeDeviceType(uint8_t deviceType) {
    switch (deviceType) {
        case 0x29: return "Asset Tracker";
        case 0x78: return "Heart Rate Monitor (HRM)";
        case 0x7B: return "Bike Speed Sensor";
        case 0x7C: return "Bike Speed/Cadence Sensor";
        case 0x0F: return "Generic GPS (Garmin)";
        case 0x30: return "Temperature Sensor";
        case 0x0D: return "Stride Sensor";
        case 0x79: return "Garmin Dog Collar (proprietary)";
        default: {
            std::ostringstream oss;
            oss << "Unknown (0x" << std::hex << std::uppercase << static_cast<int>(deviceType) << ")";
            return oss.str();
        }
    }
}

inline std::string formatDeviceInfo(const USHORT deviceId, const UCHAR deviceType, const UCHAR txType) {
    std::ostringstream oss;

    oss << "Device ID: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(deviceId)
        << " '" << std::dec << static_cast<int>(deviceId) << "' | ";

    oss << "Device Type: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(deviceType)
        << std::dec << " '" << describeDeviceType(deviceType) << "' | ";

    oss << "Transmission Type: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(txType);

    return oss.str();
}

#endif // ANT_DEVICE_H

