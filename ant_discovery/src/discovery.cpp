#include "types.h"
#include "dsi_framer_ant.hpp"
#include "dsi_serial_generic.hpp"
#include "ant.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <set>
#include <string>
#include <map>

constexpr UCHAR USER_CHANNEL_HRM = 0;
constexpr UCHAR DEVICE_TYPE_HRM = 0x78;

constexpr UCHAR USER_CHANNEL_ASSET = 1;
constexpr UCHAR DEVICE_TYPE_ASSET_TRACKER = 0x29;

constexpr UCHAR USER_NETWORK_NUM = 0;
constexpr UCHAR USER_CHANNEL_RF_FREQ = 57;
constexpr ULONG MESSAGE_TIMEOUT = 1000;
constexpr UCHAR TRANSMISSION_TYPE_WILDCARD = 0x00;

static UCHAR USER_NETWORK_KEY[8] = {
    0xB9, 0xA5, 0x21, 0xFB,
    0xBD, 0x72, 0xC3, 0x45
};

// Page constants
constexpr uint8_t PAGE_LOCATION_1 = 0x01;
constexpr uint8_t PAGE_LOCATION_2 = 0x02;
constexpr uint8_t PAGE_BATTERY_STATUS = 0x03;
constexpr uint8_t PAGE_IDENTIFICATION = 0x10;
constexpr uint8_t PAGE_IDENTIFICATION_1 = 0x11;
constexpr uint8_t PAGE_IDENTIFICATION_2 = 0x12;
constexpr uint8_t PAGE_REQUEST = 0x46;

namespace ant {

    enum class LogLevel {
        Fine,
        Info,
        Warn,
        Error
    };

    struct Device {
        uint8_t index;
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
    };

    struct ExtendedInfo {
        bool hasRssi = false;
        bool hasProximity = false;
        bool hasTxType = false;
        bool hasDevType = false;

        int8_t rssi = 0;         // in dBm (can be negative)
        uint8_t proximity = 0;   // Proximity (0–255)
        uint8_t txType = 0;
        uint8_t devType = 0;

        uint8_t trailerLength = 0;
    };


    enum class AntProfile {
        Unknown,
        AssetTracker,
        HeartRate,
        GarminProprietary
    };

    static DSIFramerANT *pclANT = nullptr;
    static DSISerialGeneric *pclSerial = nullptr;
    static std::set<uint8_t> knownIndexes;
    static std::set<uint8_t> discoveredIndexes;
    static std::set<uint16_t> configuredDevIds;
    static std::map<uint8_t, std::string> partialNames;
    static std::map<uint8_t, int> firstPageCounters;
    static auto logLevel = LogLevel::Info;

    void log(const LogLevel level, const std::string& message) {
        if (level < logLevel) return;

        auto name = "";
        switch (level) {
            case LogLevel::Fine:  name = "[FINE]"; break;
            case LogLevel::Info:  name = "[INFO]"; break;
            case LogLevel::Warn:  name = "[WARN]"; break;
            case LogLevel::Error: name = "[ERROR]"; break;
        }

        const auto now = std::chrono::system_clock::now();
        const std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        const std::tm* tm_ptr = std::localtime(&now_c);
        char timeBuf[20];
        std::strftime(timeBuf, sizeof(timeBuf), "%F %T", tm_ptr);

        std::cout << timeBuf << ": " << name << ": " << message << std::endl;
    }

    void fine(const std::string& message) {
        log(LogLevel::Fine, message);
    }

    void info(const std::string& message) {
        log(LogLevel::Info, message);
    }

    void warn(const std::string& message) {
        log(LogLevel::Warn, message);
    }

    void error(const std::string& message) {
        log(LogLevel::Error, message);
    }

    std::string toHex(const uint8_t* d, int length) {
        std::ostringstream oss;
        for (int i = 0; i < length; ++i) {
            oss << std::uppercase << std::hex
                << std::setw(2) << std::setfill('0')
                << static_cast<int>(d[i]) << " ";
        }
        return oss.str();
    }

    std::string toHexByte(const uint8_t byte) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        return oss.str();
    }

    std::string describeDeviceType(uint8_t deviceType) {
        switch (deviceType) {
            case 0x78: return "Heart Rate Monitor (HRM)";
            case 0x7C: return "Bike Speed/Cadence Sensor";
            case 0x0F: return "Generic GPS (Garmin)";
            case 0x29: return "Asset Tracker";
            case 0x79: return "Garmin Proprietary (legacy Asset Tracker?)";
            default: {
                std::ostringstream oss;
                oss << "Unknown (0x" << std::hex << std::uppercase << static_cast<int>(deviceType) << ")";
                return oss.str();
            }
        }
    }

    std::string formatDeviceInfo(const USHORT deviceId, const UCHAR deviceType, const UCHAR txType) {
        std::ostringstream oss;

        oss << "Device ID: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(deviceId)
            << " '" << std::dec << static_cast<int>(deviceId) << "' | ";

        oss << "Device Type: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(deviceType)
            << std::dec << " '" << describeDeviceType(deviceType) << "' | ";

        oss << "Transmission Type: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(txType);

        return oss.str();
    }

    ExtendedInfo parseExtendedInfo(const uint8_t* trailerStart, uint8_t flags) {
        ExtendedInfo info;
        size_t offset = 0;

        if (flags & (1 << 4)) { // Proximity
            info.proximity = trailerStart[offset++];
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
            info.txType = trailerStart[offset++];
            info.hasTxType = true;
        }

        if (flags & (1 << 0)) { // DevType
            info.devType = trailerStart[offset++];
            info.hasDevType = true;
        }

        info.trailerLength = static_cast<uint8_t>(offset);
        return info;
    }

    uint8_t trailerLengthGuess(uint8_t flags) {
        uint8_t len = 0;
        if (flags & (1 << 4)) len++; // Proximity
        if (flags & (1 << 3)) len++; // RSSI
        if (flags & (1 << 2)) len++; // Channel Type
        if (flags & (1 << 1)) len++; // TxType
        if (flags & (1 << 0)) len++; // DevType
        return len;
    }


    bool initialize(const UCHAR ucDeviceNumber) {
        info("ANT initialization started...");

        pclSerial = new DSISerialGeneric();
        if (!pclSerial->Init(50000, ucDeviceNumber)) {
            std::ostringstream oss;
            oss << "Failed to open USB port " << static_cast<int>(ucDeviceNumber);
            info(oss.str());
            return false;
        }
        pclANT = new DSIFramerANT(pclSerial);
        pclSerial->SetCallback(pclANT);
        if (!pclANT->Init()) {
            info("Framer Init failed");
            return false;
        }
        if (!pclSerial->Open()) {
            info("Serial Open failed");
            return false;
        }

        pclANT->ResetSystem();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        while (true) {
            const USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);
            if (length > 0 && length != DSI_FRAMER_TIMEDOUT) {
                ANT_MESSAGE msg;
                pclANT->GetMessage(&msg);
                std::ostringstream oss;
                oss << "Message ID was " << static_cast<int>(msg.ucMessageID);
                info(oss.str());
                if (msg.ucMessageID == MESG_STARTUP_MESG_ID) break;
            }
        }
        return true;
    }

    bool startDiscovery() {
        info("Start ANT discovery...");

        if (!pclANT->SetNetworkKey(USER_NETWORK_NUM, USER_NETWORK_KEY, MESSAGE_TIMEOUT)) {
            error("SetNetworkKey failed");
            return false;
        }

        if (true) {
            // --- Channel 0: HRM (2457 MHz = ch 57) ---
            if (!pclANT->AssignChannel(USER_CHANNEL_HRM, /*slave=*/0x00, /*no ack=*/USER_NETWORK_NUM, MESSAGE_TIMEOUT)) {
                error("AssignChannel failed");
                return false;
            }
            if (!pclANT->SetChannelID(USER_CHANNEL_HRM, 0, DEVICE_TYPE_HRM, TRANSMISSION_TYPE_WILDCARD, MESSAGE_TIMEOUT)) {
                error("SetChannelID failed");
                return false;
            }

            // Heart Rate message period
            if (!pclANT->SetChannelPeriod(USER_CHANNEL_HRM, 8070, MESSAGE_TIMEOUT)) {
                error("SetChannelPeriod failed");
                return false;
            }
            if (!pclANT->SetChannelRFFrequency(USER_CHANNEL_HRM, USER_CHANNEL_RF_FREQ, MESSAGE_TIMEOUT)) {
                error("SetChannelRFFrequency failed");
                return false;
            }
            if (!pclANT->SetChannelSearchTimeout(USER_CHANNEL_HRM, USER_CHANNEL_RF_FREQ, 0xFF)) {
                error("SetChannelSearchTimeout failed");
                return false;
            }
            if (!pclANT->OpenChannel(USER_CHANNEL_HRM, MESSAGE_TIMEOUT)) {
                error("OpenChannel failed");
                return false;
            }
        }

        if (true) {
            // --- Channel 1: Garmin Asset/GPS (2457 MHz = ch 57), slave=0x03 (shared + bi-directional) ---
            if (!pclANT->AssignChannel(USER_CHANNEL_ASSET, /*type=*/0x03, /*no ack=*/USER_NETWORK_NUM, MESSAGE_TIMEOUT)) {
                error("AssignChannel failed");
                return false;
            }
            if (!pclANT->SetChannelID(USER_CHANNEL_ASSET, 0, DEVICE_TYPE_ASSET_TRACKER, TRANSMISSION_TYPE_WILDCARD, MESSAGE_TIMEOUT)) {
                error("SetChannelID failed");
                return false;
            }
            // Asset Tracker message period
            if (!pclANT->SetChannelPeriod(USER_CHANNEL_ASSET, 2048, MESSAGE_TIMEOUT)) {
                error("SetChannelPeriod failed");
                return false;
            }
            if (!pclANT->SetChannelRFFrequency(USER_CHANNEL_ASSET, USER_CHANNEL_RF_FREQ, MESSAGE_TIMEOUT)) {
                error("SetChannelRFFrequency failed");
                return false;
            }
            if (!pclANT->SetChannelSearchTimeout(USER_CHANNEL_ASSET, USER_CHANNEL_RF_FREQ, 0x07)) {
                error("SetChannelSearchTimeout failed");
                return false;
            }
            if (!pclANT->OpenChannel(USER_CHANNEL_ASSET, MESSAGE_TIMEOUT)) {
                error("OpenChannel failed");
                return false;
            }
        }

        pclANT->RxExtMesgsEnable(TRUE);
        info("Discovery started successfully!");
        return true;
    }

    void requestAssetPage(const uint8_t page, const uint8_t index) {
        uint8_t request[8] = {
            PAGE_REQUEST,      // Data Page Number = 0x46
            0x00,              // Reserved
            page,              // Requested page ID
            0x01,              // Transmission response = send until acknowledged (broadcast is 0 = no ack)
            0x00,              // Request count = 0 (indefinite)
            index,             // Index (target subfield or asset; e.g. dog ID / sub-index / 0 for all)
            0xFF, 0xFF         // Wildcard Device Number (request all)
        };
        {
            const bool ok = pclANT->SendAcknowledgedData(USER_CHANNEL_ASSET, request);
            std::ostringstream oss;
            oss << "SendAcknowledgedData returned: " << (ok ? "OK" : "FAIL");
            fine(oss.str());
        }
        {
            std::ostringstream oss;
            oss << "Requested asset page 0x" << toHexByte(page) << " from #" << static_cast<int>(index);
            info(oss.str());
        }
    }

    void requestAssetIdentification(const uint8_t page, const uint8_t index) {
        uint8_t request[8] = {
            PAGE_REQUEST,       // 0
            0x00,               // Reserved
            page,               // Page ID
            0x01,               // Transmission response = send until acknowledged (broadcast is 0 = no ack)
//            0x00,                // Transmission Response = send once
            0x00,               // Request count = 0 (indefinite)
            index,              // Index (target subfield or asset; e.g. dog ID / sub-index / 0 for all)
            0xFF, 0xFF          // Wildcard device number
        };

        {
            const bool ok = pclANT->SendAcknowledgedData(USER_CHANNEL_ASSET, request);
            std::ostringstream oss;
            oss << "SendAcknowledgedData returned: " << (ok ? "OK" : "FAIL");
            fine(oss.str());
        }
        {
            std::ostringstream oss;
            oss << "Requested identification page 0x" << toHexByte(page) << " from #" << static_cast<int>(index);
            info(oss.str());
        }
    }

    void handleAssetIdentification(const uint8_t* data) {
        Device d;
        d.index = data[1] & 0x1F;
        d.icon  = (data[1] & 0xE0) >> 5;
        d.fullName = std::string(reinterpret_cast<const char*>(&data[2]), 8);
        d.fullName.erase(d.fullName.find('\0'));
        d.fullName.erase(d.fullName.find_last_not_of(" ") + 1);
        partialNames[d.index] = d.fullName;

        {
            std::ostringstream oss;
            oss << "[Asset/3] #" << static_cast<int>(d.index)
                    << " name='" << d.fullName << "' icon=" << static_cast<int>(d.icon);
            info(oss.str());
        }
    }

    void handleIdentificationPage1(const uint8_t* data) {
        const uint8_t index = data[1] & 0x1F;
        firstPageCounters[index]++;
        if (firstPageCounters[index] > 3) {
            std::ostringstream oss;
            oss << "[Asset] #" << static_cast<int>(index) << " seems disconnected → removing";
            info(oss.str());
            knownIndexes.erase(index);
            partialNames.erase(index);
            firstPageCounters.erase(index);
            return;
        }
        partialNames[index] = std::string(reinterpret_cast<const char*>(&data[2]), 8);
    }

    void handleIdentificationPage2(const uint8_t* data) {
        Device d;
        d.index = data[1] & 0x1F;
        firstPageCounters[d.index] = 0;
        std::string part2(reinterpret_cast<const char*>(&data[2]), 8);
        d.fullName = partialNames[d.index] + part2;

        {
            std::ostringstream oss;
            oss << "[Asset/11] #" << static_cast<int>(d.index)
                << " full name='" << d.fullName;
            info(oss.str());
        }
    }


    void logRawPayloadIfAssetTracker(const uint8_t deviceType, const uint8_t* data, const uint8_t length) {
        if (deviceType != 0x29) return;

        std::ostringstream oss;
        oss << "[RAW Asset Tracker] Payload (" << static_cast<int>(length) << " bytes): ";
        for (uint8_t i = 0; i < length; ++i) {
            oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                << static_cast<int>(data[i]) << ' ';
        }
        fine(oss.str()); // or info(oss.str()) if you want to always show it
    }


    void handleLocationPage1(const uint8_t* data, const uint8_t length) {
        Device d;
        d.index = data[1] & 0x1F;
        firstPageCounters[d.index]++;
        if (firstPageCounters[d.index] > 3) {
            std::ostringstream oss;
            oss << "[Asset] #" << static_cast<int>(d.index) << " seems disconnected → removing";
            info(oss.str());
            knownIndexes.erase(d.index);
            partialNames.erase(d.index);
            firstPageCounters.erase(d.index);
            return;
        }
        d.distance = data[2] | (data[3] << 8);
        const float bearingBradians = data[4] / 256.0f * 2.0f * static_cast<float>(M_PI);
        d.headingDegrees = bearingBradians * (180.0f / static_cast<float>(M_PI));

        const uint8_t status = data[5];
        d.gpsLost    = status & 0x01;
        d.commsLost  = status & 0x02;
        d.remove     = status & 0x04;
        d.lowBattery = status & 0x08;

        {
            std::ostringstream oss;
            oss << "[Asset/1] #" << static_cast<int>(d.index);
            auto it = partialNames.find(d.index);
            if (it != partialNames.end()) {
                oss << " (\"" << it->second << "\")";
            }

            if (d.distance == 0xFFFF) {
                oss << " → ?";
            } else {
                oss << " → " << d.distance;
            }
            oss << "m @ " << std::fixed << std::setprecision(1) << d.headingDegrees << "°";

            if (d.gpsLost)     oss << " | GPS Lost";
            if (d.commsLost)   oss << " | Comms Lost";
            if (d.remove)      oss << " | Remove";
            if (d.lowBattery)  oss << " | Battery Low";

            if (length >= 11) {
                const uint16_t devId = data[9] | (data[10] << 8);
                uint8_t flags = 0;
                uint8_t devType = 0;
                uint8_t txType = 0;
                ExtendedInfo ext = {};

                if (length >= 13) {
                    flags = data[length - 1];
                    const uint8_t* trailer = data + (length - 1 - trailerLengthGuess(flags));
                    ext = parseExtendedInfo(trailer, flags);
                    devType = ext.devType;
                    txType = ext.txType;
                }
                oss << " | Trailer bytes used: " << static_cast<int>(ext.trailerLength);

                oss << " | "<< formatDeviceInfo(devId, devType, txType);

                if (ext.hasRssi) oss << " | RSSI: " << static_cast<int>(ext.rssi) << " dBm";
                if (ext.hasProximity) oss << " | Proximity: " << static_cast<int>(ext.proximity);

                oss << " | Flags: 0x" << std::hex << static_cast<int>(flags) << std::dec;

            }
            info(oss.str());

        }
        if (knownIndexes.insert(d.index).second) {
            requestAssetPage(PAGE_BATTERY_STATUS, d.index);
            requestAssetIdentification(PAGE_IDENTIFICATION, d.index);
            requestAssetIdentification(PAGE_IDENTIFICATION_1, d.index);
            requestAssetIdentification(PAGE_IDENTIFICATION_2, d.index);
        }

        logRawPayloadIfAssetTracker(DEVICE_TYPE_ASSET_TRACKER, data, length);
    }

    void handleLocationPage2(const uint8_t* data) {
        Device d;
        d.index = data[1] & 0x1F;
        firstPageCounters[d.index] = 0;
        requestAssetIdentification(PAGE_IDENTIFICATION, d.index);

        const int32_t lat = data[2] | data[3] << 8 | data[4] << 16 | data[5] << 24;
        const int32_t lon = data[6] | data[7] << 8 | data[8] << 16 | data[9] << 24;

        d.latitude  = lat * (180.0 / 0x80000000);
        d.longitude = lon * (180.0 / 0x80000000);

        {
            std::ostringstream oss;
            oss << "[Asset/2] #" << static_cast<int>(d.index);
            const auto it = partialNames.find(d.index);
            if (it != partialNames.end()) {
                oss << " (\"" << it->second << "\")";
            }

            oss << " @ " << std::fixed << std::setprecision(6)
                << d.latitude << ", " << d.longitude;
            info(oss.str());
        }
    }

    void handleBatteryStatus(const uint8_t* data) {
        Device d;
        d.index = data[1] & 0x1F;
        const uint8_t battery = data[2];

        switch (battery) {
            case 1: d.batteryLevel = "New"; break;
            case 2: d.batteryLevel = "Good"; break;
            case 3: d.batteryLevel = "Ok"; break;
            case 4: d.batteryLevel = "Low"; break;
            case 5: d.batteryLevel = "Critical"; break;
            default: d.batteryLevel = "Unknown";
        }

        {
            std::ostringstream oss;
            oss << "[Asset/6] #" << static_cast<int>(d.index)
                << " Battery: " << d.batteryLevel;
            info(oss.str());
        }
    }

    void handleUnknownPage(const uint8_t* data) {
        const uint8_t page = data[0];
        std::ostringstream oss;
        oss << "[Asset/?] Unknown page: 0x" << std::hex << static_cast<int>(page);
    }

    void onAssetTrackerMessage(const uint8_t* data, const uint8_t length) {
        if (length < 2) return;
        const uint8_t page = data[0];

        switch (page) {
            case PAGE_LOCATION_1:         handleLocationPage1(data, length); break;
            case PAGE_LOCATION_2:         handleLocationPage2(data); break;
            case PAGE_IDENTIFICATION:     handleAssetIdentification(data); break;
            case PAGE_BATTERY_STATUS:     handleBatteryStatus(data); break;
            case PAGE_IDENTIFICATION_1:   handleIdentificationPage1(data); break;
            case PAGE_IDENTIFICATION_2:   handleIdentificationPage2(data); break;
            default:                      handleUnknownPage(data); break;
        }
    }

    void onHeartRateMessage(const uint8_t* d, const uint8_t length) {
        // Assume standard ANT+ HRM always
        const uint8_t hr = static_cast<int>(d[8]);

        if (hr < 30 || hr > 220) {
            std::ostringstream oss;
            oss << "[Ignored] Implausible HR: " << static_cast<int>(hr) << " bpm";
            info(oss.str());
            return;
        }

        {
            std::ostringstream oss;
            oss << "Heart Rate: " << static_cast<int>(hr) << " bpm";

            if (length >= 11) {
                const uint16_t devId = d[9] | (d[10] << 8);
                uint8_t flags = 0;
                uint8_t devType = 0;
                uint8_t txType = 0;
                ExtendedInfo ext = {};

                if (length >= 13) {
                    flags = d[length - 1];
                    const uint8_t* trailer = d + (length - 1 - trailerLengthGuess(flags));
                    ext = parseExtendedInfo(trailer, flags);
                    devType = ext.devType;
                    txType = ext.txType;
                }
                oss << " | Trailer bytes used: " << static_cast<int>(ext.trailerLength);

                oss << " | "<< formatDeviceInfo(devId, devType, txType);

                if (ext.hasRssi) oss << " | RSSI: " << static_cast<int>(ext.rssi) << " dBm";
                if (ext.hasProximity) oss << " | Proximity: " << static_cast<int>(ext.proximity);

                oss << " | Flags: 0x" << std::hex << static_cast<int>(flags) << std::dec;

                info(oss.str());
            }
        }
    }

    void onGenericMessage(const uint8_t* d, const uint8_t length) {
        info("Generic ANT+ payload: " + toHex(d, length));
        if (length >= 11) {
            std::ostringstream oss;
            const uint16_t devId = d[9] | (d[10] << 8);
            oss << "[Generic] Device ID: " << devId;
            info(oss.str());
        }

        // Extract and set ChannelID if possible
        if (length >= 13) {
            const uint16_t devId = d[9] | (d[10] << 8);
            const uint8_t txType = d[11];
            const uint8_t devType = d[12];

            if (configuredDevIds.insert(devId).second) {
                std::ostringstream oss;
                oss.clear();
                oss << "[ANT] Setting specific Channel ID → devId=" << devId
                          << " devType=0x" << std::hex << static_cast<int>(devType)
                          << " txType=0x" << static_cast<int>(txType) << std::dec;

                pclANT->SetChannelID(USER_CHANNEL_ASSET, devId, devType, txType, MESSAGE_TIMEOUT);

                info(oss.str());
            }

        }

        if (length >= 2 && d[0] == 0x00) {
            const uint8_t index = d[1] & 0x1F;
            if (discoveredIndexes.insert(index).second) {
                std::ostringstream oss;
                oss << "[Generic] Page 0 received from #" << static_cast<int>(index)
                    << " → requesting identification...";
                info(oss.str());
                requestAssetIdentification(PAGE_IDENTIFICATION, index);
            }
        }
    }


    void onGarminProprietary(const uint8_t* d, const uint8_t length) {
        info("[GarminProprietary] Suspicious packet: " + toHex(d, length));
    }

    void dumpRaw(const UCHAR ucChannel, const UCHAR ucMessageID, const UCHAR* d, const UCHAR length) {
        std::ostringstream oss;
        oss << "Channel [" << static_cast<int>(ucChannel) <<  "]: "
            << "Message (id=0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(ucMessageID) << "): " ;

        USHORT deviceId = 0;
        UCHAR deviceType = 0;
        UCHAR txType = 0;
        pclANT->GetChannelID(ucChannel, &deviceId, &deviceType, &txType);

        oss << formatDeviceInfo(deviceId, deviceType, txType) << " | ";
        oss << "Raw payload (" << std::dec << static_cast<int>(length) << "): " << toHex(d, length);

        fine(oss.str());
    }

    AntProfile detectProfile(const ANT_MESSAGE& msg, const uint8_t length) {
        if (msg.ucMessageID != MESG_BROADCAST_DATA_ID &&
            msg.ucMessageID != MESG_EXT_BROADCAST_DATA_ID)
            return AntProfile::Unknown;

        const uint8_t* d = msg.aucData;

        // Detect Asset Tracker pages
        if (length >= 10 && (d[0] == 0x01 || d[0] == 0x02 || d[0] == 0x03)) {
            return AntProfile::AssetTracker;
        }

        // Detect HRM only if DevType == 0x78
        if (length >= 14) {
            const uint8_t devType = d[12];  // from extended data
            if (d[0] == 0x00 && devType == 0x78)
                return AntProfile::HeartRate;
        }

        // Suspicious patterns (noise)
        if (length == 14 && d[0] == 0x00 && d[8] == 0xFF &&
            d[5] == 0xFF && d[6] == 0xFF) {
            return AntProfile::GarminProprietary;
            }

        return AntProfile::Unknown;
    }


    void dispatchAntPlusMessage(const UCHAR ucChannel, const ANT_MESSAGE& msg, const UCHAR length) {
        dumpRaw(ucChannel, msg.ucMessageID, msg.aucData, length);
        const AntProfile profile = detectProfile(msg, length);
        switch (profile) {
            case AntProfile::HeartRate:
                onHeartRateMessage(msg.aucData, length);
                break;
            case AntProfile::AssetTracker:
                onAssetTrackerMessage(msg.aucData, length);
                break;
            case AntProfile::GarminProprietary:
                onGarminProprietary(msg.aucData, length);
                break;
            default:
                onGenericMessage(msg.aucData, length);
                break;
        }
    }

    bool searching = true;

    void runEventLoop() {
        info("Starting event loop...");
        auto lastMessageTime = std::chrono::steady_clock::now();
        while (searching) {
            auto now = std::chrono::steady_clock::now();
            const USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);

            if (length == DSI_FRAMER_TIMEDOUT || length == 0) {
                const auto secondsSinceLast = std::chrono::duration_cast<std::chrono::seconds>(now - lastMessageTime).count();
                if (secondsSinceLast > 5) {
                    std::ostringstream oss;
                    oss << "No ANT messages received in the last "
                        << secondsSinceLast
                        << " seconds";
                    info(oss.str());
                    lastMessageTime = now;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            ANT_MESSAGE msg;
            pclANT->GetMessage(&msg);

            const UCHAR ucChannel = msg.aucData[0];
            const UCHAR ucMessageID = msg.ucMessageID;

            if (ucMessageID == 0) {
                continue;
            }

            if (ucMessageID == MESG_EVENT_ID ||
                ucMessageID == MESG_RESPONSE_EVENT_ID) {
                if (false) {
                    std::ostringstream oss;
                    oss << "[ANT] Event or response received: msgID=" << std::hex << msg.ucMessageID
                            << " | Code=" << static_cast<int>(msg.aucData[2]);
                    fine(oss.str());
                }
                continue;
            }

            if (true) {
                std::ostringstream oss;
                oss << "Got Message ("
                    << "id = 0x" << std::hex << std::uppercase << std::setw(2)
                    << std::setfill('0') << static_cast<int>(ucMessageID) << ", "
                    << "len = " << std::dec << static_cast<int>(length) << ")";
                fine(oss.str());
            }

            if (ucMessageID == MESG_BROADCAST_DATA_ID ||
                ucMessageID == MESG_EXT_BROADCAST_DATA_ID) {

                lastMessageTime = now;
                dispatchAntPlusMessage(ucChannel, msg, length);

            }
        }
    }

    void cleanup() {
        searching = false;
        if (pclANT) {
            std::ostringstream oss;
            oss << "Closing ANT channel...";
            info(oss.str());
            pclANT->CloseChannel(USER_CHANNEL_HRM);
            pclANT->UnAssignChannel(USER_CHANNEL_HRM);
            pclANT->CloseChannel(USER_CHANNEL_ASSET);
            pclANT->UnAssignChannel(USER_CHANNEL_ASSET);
            pclANT->ResetSystem();
            delete pclANT;
            pclANT = nullptr;
        }
        if (pclSerial) {
            pclSerial->Close();
            delete pclSerial;
            pclSerial = nullptr;
        }
    }

} // namespace ant
