    #include "types.h"
#include "dsi_framer_ant.hpp"
#include "dsi_serial_generic.hpp"
#include "ant.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <execinfo.h>
#include <iomanip>
#include <set>
#include <string>
#include <map>
#include <unordered_set>

#include "asset_tracker_discovery.h"
#include "dsi_debug.hpp"
#include "hrm_discovery.h"

constexpr UCHAR USER_CHANNEL_ASSET = 1;

namespace ant {

    enum class LogLevel {
        Fine,
        Info,
        Warn,
        Error
    };

    struct Channel {
        bool use = true;
        uint8_t cNum = 0;
        uint8_t cType = 0;
        uint16_t dNum = 0;
        uint8_t dType = 0;
        uint8_t tType = 0;
        ushort period = 0;
        uint8_t rfFreq = 0;
        uint8_t searchTimeout = 0;
    };

    std::vector<Channel> channels = {
        {false, 0x00,  0x00, 0x00, 0x78, 0x00, 8070, 57, 0x012},  // HMR search
        {true, 0x01,  0x00, 0x2BB3, 0x78, 0x51, 8070, 57, 0x012},   // Paired HRM
        // -----------------------------------------------------------------------------
        // ANT+ Asset Tracker – Pairing Mode
        //
        // This configuration enables ANT+ pairing as defined in the Device Profile
        // "ANT+ Asset Tracker Rev 1.0", Chapter 6: Device Pairing.
        //
        // Key behaviors:
        // - The receiver (e.g., this app or a Fenix watch) opens an ANT channel using:
        //     - Device #:        0 (wildcard)
        //     - Device Type:      0x29 (Asset Tracker)
        //     - Transmission Type: 0 (wildcard, required for pairing)
        //     - Channel Period:   2048 (16 Hz) – mandatory for Asset Tracker
        //     - RF Frequency:     2457 MHz – standard for most ANT+ profiles
        //
        // - No data is transmitted by the receiver during pairing.
        // - The receiver listens passively for Location Page 0x01 messages.
        // - A valid pairing candidate must send:
        //     - Page 0x01 with a valid index, distance and bearing
        //     - Extended data ("Rx trailer") with:
        //         - Device # (2 bytes)
        //         - Device Type (0x29)
        //         - Transmission Type (any, 0x00 preferred)
        //         - Optionally RSSI / Proximity info (flags 0xD0+)
        // - Once a message is received, the receiver may cache the Device # and
        //   Transmission Type for future use (persistent pairing).
        // - To be compatible with future devices, **any Transmission Type returned
        //   is valid** and should be accepted.
        //
        // Reference:
        // ANT+ Asset Tracker Device Profile, Rev 1.0 – Section 6: Device Pairing
        // -----------------------------------------------------------------------------
        {false, 2,  0x00, 0x00, 0x29, 0x00, 2048, 57, 0x03},    // Asset search
        {true, 3,  0x00, 0x024A, 0x29, 0xD5, 2048, 57, 0x06},   // Paired Alpha 10
        {true, 4,  0x00, 0x7986, 0x29, 0x65, 2048, 57, 0x03},  // Paired Astro 320
    };

    enum class AntProfile {
        Unknown,
        HeartRate,
        AssetTracker,
    };

    std::string toString(const AntProfile s) {
        switch (s) {
        case AntProfile::Unknown: return "Unknown";
        case AntProfile::HeartRate:   return "HRM";
        case AntProfile::AssetTracker:   return "Tracker";
        default:                        return "Invalid";
        }
    }

    // Enum for situation field in status byte
    enum class AssetSituation {
        Undefined = 255,
        Unknown = 0,
        OnPoint = 1,
        Treeing = 2,
        Running = 3,
        Caught = 4,
        Barking = 5,
        Training = 6,
        Hunting = 7
    };

    struct Device {
        bool gpsLost = false;
        bool commsLost = false;
        bool remove = false;
        bool lowBattery = false;

        uint8_t index{};
        uint8_t flags = 0;
        uint8_t aType = 0;
        uint8_t dType = 0;
        uint8_t tType = 0;
        uint8_t color = 0;
        uint16_t number = 0;
        std::string uName;
        std::string lName;
        std::string fName;
        AssetSituation situation = AssetSituation::Unknown;

        uint16_t distance = 0;
        float headingDegrees = 0.0f;
        double latitude = 0.0;
        double longitude = 0.0;

        std::string batteryLevel = "?";
        std::string batteryVoltage = "?";
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

    static DSIFramerANT *pclANT = nullptr;
    static DSISerialGeneric *pclSerial = nullptr;
    static std::set<std::string> pairedDevices;
    static std::set<std::string> recentPageRequests;
    static std::map<std::string, std::set<uint8_t>> knownIndexes;
    static std::map<std::string, std::map<uint8_t, std::string>> knownNames;
    static std::map<std::string, std::map<uint8_t, uint16_t>> knownLatitudes;

    const std::unordered_set<uint8_t> assetPages = {
        PAGE_NO_ASSETS,
        PAGE_LOCATION_1,
        PAGE_LOCATION_2,
        PAGE_IDENTIFICATION_1,
        PAGE_IDENTIFICATION_2,
    };

    static const std::map<uint16_t, std::string> manufacturerMap = {
        {1, "Garmin"},
    };

    std::string lookupManufacturer(const uint16_t id) {
        const auto it = manufacturerMap.find(id);
        return it != manufacturerMap.end() ? it->second : "?";
    }

    static const std::map<uint16_t, std::map<uint16_t, std::string>> modelsMap = {
        {1, {
            {3528, "Alpha 10"},
            {1339, "Astro 320"},
        }},
    };

    std::string lookupModelName(const uint16_t id, const uint16_t number)
    {
        const auto outer = modelsMap.find(id);
        if (outer != modelsMap.end()) {
            const auto inner = outer->second.find(number);
            if (inner != outer->second.end()) {
                return inner->second;
            }
            return "?";
        }
        return "?";
    }

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

    std::string toHex(const uint8_t* d, const uint8_t length) {
        std::ostringstream oss;
        for (int i = 0; i < length; ++i) {
            oss << std::uppercase << std::hex
                << std::setw(2) << std::setfill('0')
                << static_cast<int>(d[i]) << " ";
        }
        return oss.str();
    }

    std::string toHex(const uint16_t* d, const uint8_t length) {
        std::ostringstream oss;
        for (int i = 0; i < length; ++i) {
            oss << std::uppercase << std::hex
                << std::setw(4) << std::setfill('0')
                << static_cast<int>(d[i]) << " ";
        }
        return oss.str();
    }

    std::string toHexByte(const uint8_t byte) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        return oss.str();
    }

    std::string toHexByte(const uint16_t byte) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(byte);
        return oss.str();
    }

    std::string formatUptime(uint32_t seconds) {
        const uint32_t days = seconds / 86400;
        seconds %= 86400;
        const uint32_t hours = seconds / 3600;
        seconds %= 3600;
        const uint32_t minutes = seconds / 60;
        seconds %= 60;

        std::ostringstream oss;
        if (days > 0)    oss << days << "d ";
        if (days > 0 || hours > 0)   oss << hours << "h ";
        if (days > 0 || hours > 0 || minutes > 0) oss << minutes << "m ";
        oss << seconds << "s";

        return oss.str();
    }

    std::string makeDeviceKey(const uint16_t number, const uint8_t dType, const uint8_t tType) {
        std::ostringstream oss;
        oss << number << ":" << static_cast<int>(dType) << ":" << static_cast<int>(tType);
        return oss.str();
    }

    u_int16_t parseDeviceNumber(const uint8_t* trailer) {
        return (trailer[1] << 8) + trailer[0];
    }

    std::string describeDeviceType(const uint8_t deviceType) {
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

    std::string formatDeviceInfo(const uint16_t number, const UCHAR dType, const UCHAR tType) {
        std::ostringstream oss;

        oss << "Device Type: 0x" << toHexByte(dType)
            << " '" << describeDeviceType(dType) << "'"
            << " | Device #: 0x" << toHexByte(number)
            << " | Tx Type: 0x" << toHexByte(tType);

        return oss.str();
    }

    // Assumes data format
    // Channel # (1) | Payload (8) | Flag (1) | Measurement Type (2) | RSSI Value (1) | Threshold (1) | Checksum (1)
    bool isDataRssiExt(const uint8_t* d) {
        return d[9] & RSSI_EXT_FLAG;
    }

    // Assumes data format
    // Channel # (1) | Payload (8) | Flag (1) | Device # (2) | Device Type (2) | Trans Type (1) | Checksum (1)
    bool isDataChannelIdExt(const uint8_t* d) {
        return d[9] & CHANNEL_ID_EXT_FLAG;
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

    std::string describeAssetType(const uint8_t type) {
        switch (type) {
        case 0x00: return "Tracker";
        case 0x01: return "Dog Collar";
        default: return "Reserved";
        }
    }

    // Extracts the 'situation' field (bits 5–7) from a status byte (Data Page 1)
    AssetSituation decodeSituation(const uint8_t statusByte) {
        if (statusByte == 0xFF) return AssetSituation::Undefined;
        return static_cast<AssetSituation>((statusByte >> 5) & 0x07);
    }

    std::string toString(const AssetSituation s) {
        switch (s) {
        case AssetSituation::Undefined: return "Undefined";
        case AssetSituation::Unknown:   return "Unknown";
        case AssetSituation::OnPoint:   return "On Point";
        case AssetSituation::Treeing:   return "Treeing";
        case AssetSituation::Running:   return "Running";
        case AssetSituation::Caught:    return "Caught";
        case AssetSituation::Barking:   return "Barking";
        case AssetSituation::Training:  return "Training";
        case AssetSituation::Hunting:   return "Hunting";
        default:                        return "Invalid";
        }
    }

    bool parseDevice(const uint8_t* data, Device& device){
        const uint8_t* payload = &data[1];
        const uint8_t page = payload[0];

        const bool isDevice = assetPages.count(page);

        if (isDevice) {
            device.index = payload[1] & 0x1F;
        }

        device.flags = data[9];

        if (isDataChannelIdExt(data)) {
            const uint8_t* trailer = &data[10];
            device.number = parseDeviceNumber(trailer);
            device.dType = static_cast<UCHAR>(trailer[2]);
            device.tType = static_cast<UCHAR>(trailer[3]);
        }

        const auto knownKey = makeDeviceKey(device.number, device.dType, device.tType);

        switch (page) {
            // Data Page 0x01 (Location Page 1) contains:
            //
            // - Index: sub-ID of the asset (e.g., dog 1, 2, ...)
            // - Distance & Bearing (optional)
            // - Status flags (Situation, GPS lost, comms lost, battery low, remove)

            case PAGE_LOCATION_1: {
                device.distance = payload[3] << 8 | payload[2];
                const float bearingBradians = static_cast<float>(payload[4]) / 256.0f * 2.0f * static_cast<float>(M_PI);
                device.headingDegrees = bearingBradians * (180.0f / static_cast<float>(M_PI));

                const uint8_t status = payload[5];
                device.gpsLost    = status & 0x01;
                device.commsLost  = status & 0x02;
                device.remove     = status & 0x04;
                device.lowBattery = status & 0x08;
                device.situation = decodeSituation(status);

                // Gety lower nibble of the asset’s current latitude.
                const uint16_t lat = payload[7] << 8 | payload[6];
                auto& latitudes = knownLatitudes[knownKey];
                latitudes[device.index] = lat;

                break;
            }
            case PAGE_LOCATION_2: {
                auto& latitudes = knownLatitudes[knownKey];
                const auto lower = latitudes[device.index];
                const uint32_t lat = payload[3] << 24 | payload[2]  << 16 | lower;
                const int32_t lon = payload[7] << 24  | payload[6] << 16 | payload[5] << 8 | payload[4];

                device.latitude  = lat * (180.0 / 0x80000000);
                device.longitude = lon * (180.0 / 0x80000000);

                break;
            }
            case PAGE_IDENTIFICATION_1: {

                device.color = payload[2];
                const std::string uName(reinterpret_cast<const char*>(&payload[3]), 5);

                auto& uNames = knownNames[knownKey];
                uNames[device.index] = uName;
                device.uName = uName;

                break;
            }
            case PAGE_IDENTIFICATION_2: {
                device.aType = payload[2];
                const std::string lName(reinterpret_cast<const char*>(&payload[3]), 5);
                auto& uNames = knownNames[knownKey];
                device.uName = uNames[device.index];
                device.lName = lName;
                device.fName = device.uName + lName;
                break;
            }
            default: break;
        }

        return isDevice;
    }

    // Assumes Broadcast Data message
    void dumpBroadcastRaw(const UCHAR ucMessageID, const UCHAR* d, const UCHAR length) {
        const uint8_t ucChannel = d[0];
        const uint8_t* trailer = &d[10];
        std::ostringstream oss;
        oss << "[DUMP] Channel: " << static_cast<int>(ucChannel)
            <<  " | Message ID: 0x" << toHexByte(ucMessageID);

        UCHAR tType = 0;
        UCHAR dType = 0;
        USHORT number = 0;

        const uint8_t flag = d[9];

        if (isDataChannelIdExt(d)) {
            number = parseDeviceNumber(trailer);
            dType = static_cast<UCHAR>(trailer[2]);
            tType = static_cast<UCHAR>(trailer[3]);
        }

        oss << " | Flag: " << toHexByte(flag);
        oss << " | " << formatDeviceInfo(number, dType, tType);
        oss << " | Raw Payload (" << std::dec << static_cast<int>(length) << "): " << toHex(d, length);

        fine(oss.str());
    }

    bool initialize(const UCHAR ucDeviceNumber) {

        DSIDebug::Init();
        DSIDebug::SetDebug(true);

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

    bool shouldRequestPageAgain(const Device& d, const uint8_t page) {
        const std::string key = makeDeviceKey(d.number, d.dType, d.tType) + ":" + std::to_string(page);
        if (recentPageRequests.count(key)) {
            return false;
        }
        recentPageRequests.insert(key);
        return true;
    }

    void clearRequestCacheFor(const uint16_t number, const uint8_t dType, const uint8_t tType) {
        const std::string prefix = makeDeviceKey(number, dType, tType) + ":";
        for (const int page : {0x10, 0x11}) {
            recentPageRequests.erase(prefix + std::to_string(page));
        }
    }

    bool sendBroadcastRequestDataPage(const uint8_t channel, uint8_t* data) {
        constexpr u_int8_t maxAttempts = 5;
        const u_int8_t page = data[0];
        uint8_t retries = 0;
        while (retries < maxAttempts) {
            std::ostringstream oss;
            const auto attempt = retries < 1 ? "" : " (retry " + std::to_string(retries) + ")";
            oss << "[CH] #" << std::to_string(channel) << ": "
                << "[SendBroadcastData] Data Page 0x" << toHexByte(page) << attempt;

            const bool status = pclANT->SendBroadcastData(channel, data);

            if (status) {
                oss << " | OK";
                fine(oss.str());
                return true;
            }

            const UCHAR e = pclANT->GetLastError();
            oss << " | FAILED with 0x" << toHexByte(e)
                << " (attempt " << retries+1 << " of " << maxAttempts <<")"
                << " | " << "Raw Payload (8): " << toHex(data, 8);
            warn(oss.str());

            retries++;
        }
        return false;
    }

    bool sendAcknowledgedRequestDataPage(const uint8_t channel, uint8_t* data) {
        constexpr u_int8_t maxAttempts = 5;
        const u_int8_t page = data[0];
        uint8_t retries = 0;
        while (retries < maxAttempts) {
            std::ostringstream oss;
            const auto attempt = retries < 1 ? "" : " (retry " + std::to_string(retries) + ")";
            oss << "[CH] #" << std::to_string(channel) << ": "
                << "[SendAcknowledgedData] Data Page 0x" << toHexByte(page) << attempt;

            const bool status = pclANT->SendAcknowledgedData(channel, data, MESSAGE_TIMEOUT);

            if (status) {
                oss << " | OK";
                fine(oss.str());
                return true;
            }

            const UCHAR e = pclANT->GetLastError();
            oss << " | FAILED with 0x" << toHexByte(e)
                << " (attempt " << retries+1 << " of " << maxAttempts <<")"
                << " | " << "Raw Payload (0): " << toHex(data, 8);
            warn(oss.str());

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            retries++;
        }
        return false;
    }

    bool sendRequestDataPage(const uint8_t channel, uint8_t* data) {
        return sendAcknowledgedRequestDataPage(channel, data);
    }

    // -----------------------------------------------------------------------------
    // requestAssetIdentification
    //
    // Sends a series of ANT+ page requests to retrieve the full name and metadata
    // of a given asset (e.g., dog collar or handheld).
    //
    // Pages requested with Request Data Page Set Command (0x04)
    // - 0x10 → Identification Page 1 (first 8 chars of name)
    // - 0x11 → Identification Page 2 (last 8 chars of name)
    //
    // These pages are optional and may not be supported by all devices.
    // The function uses acknowledged data messages to increase reliability.
    //
    // Called when a new asset index is seen for the first time during pairing,
    // typically from handleLocationPage1().
    //
    // Reference:
    // - ANT+ Device Profile – Tracker Rev. 1.0 (Section 4.3.5, 4.4.3)
    // -----------------------------------------------------------------------------
    void requestAssetIdentification(const uint8_t channel, const Device& device) {
        uint8_t request[8] = {
            PAGE_REQUEST,           // Page 70
            0xFF,                   // Reserved
            0xFF,                   // Reserved
            0xFF,                   // Descriptor Byte 1
            0xFF,                   // Descriptor Byte 2
            0x01,                   // Transmit once
            PAGE_IDENTIFICATION_1,  // Requested Page: Asset Identification Page 1 (0x10)
            0x04                    // Command type: Page Set (answers with 0x10 og 0x11)
        };

        if (sendRequestDataPage(channel, request)) {
            std::ostringstream oss;
            oss << "[CH] #" << std::to_string(channel) << ": "
                << "[ASSET/70] | Requested Asset Identification Pages from"
                << " Device # 0x" << toHexByte(device.number);
            info(oss.str());
        }
    }

    void requestPage(const uint8_t channel, const uint8_t page, const std::string& prefix="", const std::string& suffix_ ="") {
        uint8_t request[8] = {
            PAGE_REQUEST,       // Page 70
            0xFF,               // Reserved
            0xFF,               // Reserved
            0xFF,               // Descriptor Byte 1
            0xFF,               // Descriptor Byte 2
            0x01,               // Transmit once
            page,               // Requested Page
            0x01                // Command type: Single Page
        };

        if (sendRequestDataPage(channel, request)) {
            std::ostringstream oss;
            oss << prefix
                << " Requested Data Page 0x" << toHexByte(page)
                << " (" << std::to_string(page) << ")";
            if (!suffix_.empty()){
                oss << " | "<< suffix_;
            }
            fine(oss.str());
        }
    }

    void requestAssetPages(const uint8_t channel, const Device& device) {
        if (true) {
            // Request asset name (0x10 and 0x11)
            if (shouldRequestPageAgain(device, PAGE_LOCATION_1)) {
                requestAssetIdentification(channel, device);
            }
        }

        const std::string prefix = "[ASSET] #" + std::to_string(device.index);
        const std::string suffix = formatDeviceInfo(device.number, device.dType, device.tType);

        // Request battery status (page 0x50)
        if (shouldRequestPageAgain(device, PAGE_MANUFACTURER_IDENT)) {
            requestPage(channel, PAGE_MANUFACTURER_IDENT, prefix, suffix);
        }

        // Request battery status (page 0x51)
        if (shouldRequestPageAgain(device, PAGE_PRODUCT_INFO)) {
            requestPage(channel, PAGE_PRODUCT_INFO, prefix, suffix);
        }

        // Request battery status (page 0x52)
        if (shouldRequestPageAgain(device, PAGE_BATTERY_STATUS)) {
            requestPage(channel,PAGE_BATTERY_STATUS, prefix, suffix);
        }
    }

    void handleManufacturerInfoPage(const uint8_t* data) {
        const uint8_t* payload = &data[1];

        const uint8_t hwRevision = payload[3];
        const uint16_t id = static_cast<uint16_t>(payload[4]) | (payload[5] << 8);
        const uint16_t number = static_cast<uint16_t>(payload[6]) | (payload[7] << 8);

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/80] Manufacturer Info: HW Revision " << static_cast<int>(hwRevision)
            << " | Manufacturer: " << lookupManufacturer(id) << " (" << id << ")"
            << " | Model: " << lookupModelName(id, number) << " (" << number << ")";

        info(oss.str());
    }

    double parseSwVersion(const uint8_t supplemental, const uint8_t main) {
        if (supplemental != 0xFF) {
            return (main * 100.0 + supplemental) / 1000.0;
        } else {
            return main / 10.0;
        }
    }

    void handleProductInfoPage(const uint8_t* data) {
        const uint8_t* payload = &data[1];
        const uint8_t swRevision = payload[2];
        const uint8_t swMain  = payload[3];

        //const uint32_t = (0x00 << 24) + (msb << 16) + ( mid << 8 ) + lsb;
        const uint32_t serial = (payload[7] << 24) + (payload[6] << 16) + ( payload[5] << 8 ) + payload[4];

        const double swVersion = parseSwVersion(swRevision, swMain);

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/81] Product Info"
            << " | SW Revision "<< std::fixed << std::setprecision(3) << swVersion
            << " | Serial #" << serial;
        info(oss.str());
    }

    void handleNoAssetsPage(const uint8_t* data, const uint8_t length) {
        Device device;

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/3] ";

        if (parseDevice(data, device)) {
            oss << "No assets connected"
                << " | Flag: 0x" << toHexByte(device.flags);

            if (isDataChannelIdExt(data)) {
                oss << " | "<< formatDeviceInfo(device.number, device.dType, device.tType);
            }
            info(oss.str());

            // Remove all existing indexes
            const auto knownKey = makeDeviceKey(device.number, device.dType, device.tType);
            knownIndexes.erase(knownKey);

            requestAssetPages(channel, device);
            return;
        }

        oss << "Handler not found for Page 0x" << toHexByte(data[0])
            << " | Raw Payload (" << std::dec << static_cast<int>(length) << "): " << toHex(data, length);
        severe(oss.str());
    }


    // -----------------------------------------------------------------------------
    // handleLocationPage1
    //
    // Entry point for asset tracking and pairing logic in the ANT+ Asset Tracker
    // profile (Device Type 0x29).
    //
    // Called when a broadcast message with Data Page 0x01 (Location Page 1) is
    // received from a tracker. This page contains:
    //
    // - Index: sub-ID of the asset (e.g., dog 1, 2, ...)
    // - Distance & Bearing (optional)
    // - Status flags (GPS lost, comms lost, battery low, remove)
    // - Extended data (Rx trailer): includes Device #, Device Type, Tx Type, etc.
    //
    // Responsibilities of this function:
    //  1. Tracks how many times this page has been received from each index
    //     → used for timeout/removal logic
    //  2. Initiates discovery of additional information (battery + identification)
    //     the first time a new asset index is seen
    //     → sends requestAssetPage() and requestAssetIdentification()
    //  3. Logs key info about the asset’s location and state
    //
    // This is the first message received when a new Asset becomes visible
    // over ANT+. It effectively “starts the conversation” between the receiver
    // and the tracker.
    //
    // Reference:
    // - ANT+ Device Profile – Tracker Rev. 1.0 (Section 4.3 and 6)
    // -----------------------------------------------------------------------------
    void handleLocationPage1(const uint8_t* data) {
        Device device;
        parseDevice(data, device);

        // Register device as paired
        const auto knownKey = makeDeviceKey(device.number, device.dType, device.tType);
        pairedDevices.insert(knownKey);

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/1] #" << static_cast<int>(device.index);

        if (device.distance == 0xFFFF) {
            oss << " → ?";
        } else {
            oss << " → " << device.distance;
        }
        oss << "m @ " << std::fixed << std::setprecision(1) << device.headingDegrees << "°";

        // Find known (upper) name
        auto& uNames = knownNames[knownKey];
        const auto name = uNames[device.index];
        if (!name.empty()) {
            oss << " | " << name;
        }

        if (device.gpsLost)     oss << " | GPS Lost";
        if (device.commsLost)   oss << " | Comms Lost";
        if (device.remove)      oss << " | Remove";
        if (device.lowBattery)  oss << " | Battery Low";
        oss << " | " << toString(device.situation)
            << " | Flag: 0x" << toHexByte(device.flags);
        if (isDataChannelIdExt(data)){
            oss << " | "<< formatDeviceInfo(device.number, device.dType, device.tType);
        }
        info(oss.str());

        auto& assets = knownIndexes[knownKey];
        if (assets.insert(device.index).second) {
            requestAssetPages(channel, device);
        }
    }

    void handleLocationPage2(const uint8_t* data) {
        Device device;
        parseDevice(data, device);

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/2] #" << static_cast<int>(device.index)
            << " @ " << std::fixed << std::setprecision(6)
            << device.latitude << ", " << device.longitude
            << " | Flag: 0x" << toHexByte(device.flags);

        if (isDataChannelIdExt(data)){
            oss << " | "<< formatDeviceInfo(device.number, device.dType, device.tType);
        }

        info(oss.str());
    }

    // Broadcast message format:
    // d[0] = channel number
    // d[1..8] = payload
    // d[9] = flag (0x80)
    // d[10..13] = trailer: device number (LSB, MSB), device type, transmission type
    void handleIdentificationPage1(const uint8_t* data) {
        Device device;
        parseDevice(data, device);

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/16] #" << static_cast<int>(device.index)
            << " | Upper Name: " << device.uName
            << " | Color: " << static_cast<int>(device.color)
            << " | Flag: 0x" << toHexByte(device.flags);

        if (isDataChannelIdExt(data)){
            oss << " | "<< formatDeviceInfo(device.number, device.dType, device.tType);
        }

        info(oss.str());
    }

    // Broadcast message format:
    // d[0] = channel number
    // d[1..8] = payload
    // d[9] = flag (0x80)
    // d[10..13] = trailer: device number (LSB, MSB), device type, transmission type
    void handleIdentificationPage2(const uint8_t* data) {
        Device device;
        parseDevice(data, device);

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/17] #" << static_cast<int>(device.index)
            << " | Full Name: " << device.fName
            << " | Asset Type: " << describeAssetType(device.aType);

        if (isDataChannelIdExt(data)){
            oss << " | "<< formatDeviceInfo(device.number, device.dType, device.tType);
        }

        info(oss.str());
    }

    void handleBatteryStatusPage(const uint8_t* data) {
        const uint8_t* payload = &data[1];
        const uint8_t batteryId = payload[2];

        // Operating time (little-endian)
        const uint32_t ticks = (0x00 << 24) + (payload[5] << 16) + ( payload[4] << 8 ) + payload[3];

        const uint8_t fractionalVoltageRaw = payload[6];
        const uint8_t descriptiveBitField = payload[7];

        const uint8_t coarseVoltage = descriptiveBitField & 0x0F;
        const uint8_t batteryStatus = (descriptiveBitField >> 4) & 0x07;
        const bool twoSecondResolution = descriptiveBitField & 0x80;

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/82] Battery Status | Battery ID: " << static_cast<int>(batteryId) << " | ";

        // Voltage calculation
        if (coarseVoltage == 0x0F) {
            oss << "Voltage: Invalid | ";
        } else {
            double voltage = coarseVoltage + fractionalVoltageRaw / 256.0;
            oss << std::fixed << std::setprecision(3) << "Voltage: " << voltage << " V | ";
        }

        // Battery status
        oss << "Status: ";
        switch (batteryStatus) {
        case 1: oss << "New"; break;
        case 2: oss << "Good"; break;
        case 3: oss << "Ok"; break;
        case 4: oss << "Low"; break;
        case 5: oss << "Critical"; break;
        default: oss << "Reserved"; break;
        }

        // Uptime calculation
        uint32_t operatingSeconds = ticks * (twoSecondResolution ? 2 : 16);

        const int hours = operatingSeconds / 3600;
        const int minutes = (operatingSeconds % 3600) / 60;
        const int seconds = operatingSeconds % 60;

        oss << " | Uptime: ";
        if (hours > 0) oss << hours << "h ";
        if (minutes > 0 || hours > 0) oss << minutes << "m ";
        oss << seconds << "s";

        info(oss.str());
    }

    void handleUnknownPage(const uint8_t* data, const UCHAR length) {
        const uint8_t* payload = &data[1];
        const uint8_t page = payload[0];

        std::ostringstream oss;
        const uint8_t channel = data[0];
        oss << "[CH] #" << std::to_string(channel) << ": "
            << "[ASSET/?] Unknown page : 0x" << toHexByte(page);

        Device device{};
        if (parseDevice(data, device)) {
            if (isDataChannelIdExt(data)) {
                oss << " | " << formatDeviceInfo(device.number, device.dType, device.tType);
            }
        }
        oss << " | " << "Raw Payload (" << std::dec << static_cast<int>(length) << "): " << toHex(data, length);

        info(oss.str());

        requestAssetPages(channel, device);


        /*
        try {
            // TODO
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown Exception" << std::endl;
        }
        */
    }

    // -----------------------------------------------------------------------------
    // onAssetTrackerMessage
    //
    // Dispatch handler for all received ANT+ Asset Tracker messages.
    // Called from the event loop whenever a broadcast message is received on the
    // Asset Tracker channel.
    //
    // This function inspects the first byte of the payload (`data[0]`) to determine
    // which ANT+ “data page” the message represents, and delegates processing
    // accordingly.
    //
    // Pages handled:
    // - 0x01 → Location Page 1          → handleLocationPage1()
    // - 0x02 → Location Page 2          → handleLocationPage2()
    // - 0x03 → Battery/Status           → handleBatteryStatus()
    // - 0x10 → Identification (Name 1)  → handleIdentificationPage1()
    // - 0x11 → Identification (Name 2)  → handleIdentificationPage2()
    // - 0x06 → Battery Status (legacy)  → handleBatteryStatus()
    // - Default                         → handleUnknownPage()
    //
    // This function is low-level and delegates quickly — no validation is done here.
    // Page-specific logic (e.g., pairing, name parsing) lives in the individual
    // handlers.
    //
    // Reference:
    // - ANT+ Device Profile – Tracker Rev. 1.0 (Section 4.2, 4.3)
    // -----------------------------------------------------------------------------
    void onAssetTrackerMessage(const uint8_t* data, const UCHAR length) {
        const uint8_t page = data[1];

        switch (page) {
            case PAGE_NO_ASSETS:          handleNoAssetsPage(data, length); break;
            case PAGE_LOCATION_1:         handleLocationPage1(data); break;
            case PAGE_LOCATION_2:         handleLocationPage2(data); break;
//            case PAGE_IDENTIFICATION_1:   handleUnknownPage(data,length); break;
//            case PAGE_IDENTIFICATION_2:   handleUnknownPage(data, length); break;
            case PAGE_IDENTIFICATION_1:   handleIdentificationPage1(data); break;
            case PAGE_IDENTIFICATION_2:   handleIdentificationPage2(data); break;
            case PAGE_PRODUCT_INFO:       handleProductInfoPage(data); break;
            case PAGE_BATTERY_STATUS:     handleBatteryStatusPage(data); break;
            case PAGE_MANUFACTURER_IDENT: handleManufacturerInfoPage(data); break;
            default:                      handleUnknownPage(data, length); break;
        }
    }

    void onHeartRateMessage(const uint8_t* data, const uint8_t length) {
        const uint8_t channel = data[0];
        const uint8_t* payload = &data[1];

        const u_int8_t rawPage = payload[0];
        const uint8_t page = rawPage & 0x7F;
        //const bool toggle = (rawPage & 0x80) != 0;

        // Assume standard ANT+ HRM always
        const uint8_t hr = static_cast<int>(payload[7]);

        const uint8_t flag = data[9];
        std::ostringstream oss;
        oss << "[CH] #" << std::to_string(channel) << ":"
            << " [HRM/" << std::to_string(page) << "]"
            << " Heart Rate: " << static_cast<int>(hr) << " bpm"
            << " | Flag: " << toHexByte(flag);

        if (isDataChannelIdExt(data)) {
            const uint8_t* trailer = &data[10];
            const uint16_t number = parseDeviceNumber(trailer);
            const auto tType = static_cast<UCHAR>(trailer[2]);
            const auto dType = static_cast<UCHAR>(trailer[3]);
            oss << " | " << formatDeviceInfo(number, dType, tType);
        }

        info(oss.str());

    }

    void onGenericMessage(const ANT_MESSAGE& msg, const UCHAR length) {
        const uint8_t channel = msg.aucData[0];
        const std::string prefix = "[CH] #" + std::to_string(channel) + ": [GENERIC]";
        info(prefix + " ANT+ payload: " + toHex(msg.aucData, length));
        requestPage(channel, PAGE_MANUFACTURER_IDENT, prefix);
        requestPage(channel, PAGE_PRODUCT_INFO, prefix);
    }


    AntProfile detectProfile(const UCHAR* d) {
        const uint8_t* trailer = &d[10];

        uint8_t deviceType = 0;

        if (isDataChannelIdExt(d)) {
            deviceType = trailer[2];
        }

        // Detect Asset Tracker pages
        switch (deviceType){
        case HRM_DEVICE_TYPE:
            return AntProfile::HeartRate;
        case ASSET_TRACKER_DEVICE_TYPE:
            return AntProfile::AssetTracker;
        default:
            return AntProfile::Unknown;
        }
    }


    void dispatchBroadcastDataMessage(const ANT_MESSAGE& msg, const UCHAR length) {

        const UCHAR* d = msg.aucData;
        dumpBroadcastRaw(msg.ucMessageID, d, length);

        const AntProfile profile = detectProfile(d);

        switch (profile) {
            case AntProfile::HeartRate:
                onHeartRateMessage(d, length);
                break;
            case AntProfile::AssetTracker:
                onAssetTrackerMessage(d, length);
                break;
            default:
                onGenericMessage(msg, length);
                break;
        }
    }

    bool openChannel(const Channel& ch) {
        const std::string suffix = " failed for channel #" + std::to_string(ch.cNum);
        if (!pclANT->AssignChannel(ch.cNum, ch.cType, USER_NETWORK_NUM, MESSAGE_TIMEOUT)) {
            error("AssignChannel" + suffix);
            return false;
        }

        if (!pclANT->SetChannelID(ch.cNum, ch.dNum, ch.dType, ch.tType, MESSAGE_TIMEOUT)) {
            error("SetChannelID" + suffix);
            return false;
        }

        if (!pclANT->SetChannelPeriod(ch.cNum, ch.period,MESSAGE_TIMEOUT)) {
            error("SetChannelPeriod" + suffix);
            return false;
        }

        if (!pclANT->SetChannelRFFrequency(ch.cNum, ch.rfFreq,MESSAGE_TIMEOUT)) {
            error("SetChannelRFFrequency" + suffix);
            return false;
        }

        if (!pclANT->SetNetworkKey(USER_NETWORK_NUM, USER_NETWORK_KEY, MESSAGE_TIMEOUT)) {
            error("SetNetworkKey failed" + suffix);
            return false;
        }

        if (!pclANT->SetChannelSearchTimeout(ch.cNum, ch.searchTimeout, MESSAGE_TIMEOUT)) {
            error("SetChannelSearchTimeout" + suffix);
            return false;
        }

        if (!pclANT->OpenChannel(ch.cNum,MESSAGE_TIMEOUT)) {
            error("OpenChannel" + suffix);
            return false;
        }

        std::ostringstream oss;
        oss <<"Opened ANT Channel #" << std::to_string(ch.cNum)
            << " | Channel Type: 0x" + toHexByte(ch.cType)
            << " | Device #: 0x" + toHexByte(ch.dNum)
            << " | Device Type: 0x" + toHexByte(ch.dType)
            << " | Tx Type: 0x" + toHexByte(ch.tType);
        info(oss.str());
        return true;
    }

    bool closeChannel(const uint8_t number) {
        if (!pclANT) {
            error("Failed to close channel #"
                + std::to_string(number)
                + "[closeChannel] ANT framer is not initialized");
            return false;
        }

        if (!pclANT->CloseChannel(number)) {
            error("Failed to close channel #" + std::to_string(number));
            return false;
        }

        info("Channel #" + std::to_string(number) + " [CLOSED]");
        return true;
    }

    bool startDiscovery() {
        info("Starting ANT+ discovery...");

        if (!pclANT->SetNetworkKey(USER_NETWORK_NUM, USER_NETWORK_KEY, MESSAGE_TIMEOUT)) {
            error("SetNetworkKey failed");
            return false;
        }

        info("Opening ANT channels...");
        for (const auto& ch : channels) {
            if (!ch.use) {
                fine("Channel #" + std::to_string(ch.cNum) + " [SKIPPED]");
                continue;
            }
            openChannel(ch);
        }

        info("Opening ANT channels...DONE");
        if (!pclANT->RxExtMesgsEnable(TRUE)) {
            error("Failed to enable extended message format mode");
            return false;
        }

        info("Starting ANT+ discovery...DONE");
        return true;
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

            const UCHAR ucMessageID = msg.ucMessageID;

            if (ucMessageID == 0) {
                continue;
            }

            if (ucMessageID == MESG_EVENT_ID ||
                ucMessageID == MESG_RESPONSE_EVENT_ID) {
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
                dispatchBroadcastDataMessage(msg, length);

            }
        }
    }

    void cleanup() {
        searching = false;
        if (pclANT) {
            info("Closing ANT channels...");
            for (const auto& ch : channels) {
                if (!ch.use) {
                    fine("Channel #" + std::to_string(ch.cNum) + " [SKIPPED]");
                    continue;
                }
                closeChannel(ch.cNum);
            }

            /*
            pclANT->CloseChannel(HRM_CHANNEL);
            pclANT->UnAssignChannel(HRM_CHANNEL);
            pclANT->CloseChannel(USER_CHANNEL_ASSET);
            pclANT->UnAssignChannel(USER_CHANNEL_ASSET);
            pclANT->CloseChannel(2);
            pclANT->UnAssignChannel(2);
            */

            if(!pclANT->ResetSystem()) {
                error("Failed to reset ANT System");
            }

            delete pclANT;
            pclANT = nullptr;
        }
        if (pclSerial) {
            pclSerial->Close();
            delete pclSerial;
            pclSerial = nullptr;
        }
        info("Closing ANT channels...DONE");
    }

} // namespace ant
