#include "types.h"
#include "dsi_framer_ant.hpp"
#include "dsi_thread.h"
#include "dsi_serial_generic.hpp"
#include "ant.h"

#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <set>
#include <string>
#include <map>

constexpr UCHAR USER_CHANNEL_HRM = 0;
constexpr UCHAR USER_CHANNEL_ASSET = 1;
constexpr UCHAR USER_NETWORK_NUM = 0;
constexpr UCHAR USER_CHANNEL_RF_FREQ = 57;
constexpr ULONG MESSAGE_TIMEOUT = 1000;
static UCHAR USER_NETWORK_KEY[8] = {
    0xB9, 0xA5, 0x21, 0xFB,
    0xBD, 0x72, 0xC3, 0x45
};

// Page constants
constexpr uint8_t PAGE_LOCATION_1 = 0x01;
constexpr uint8_t PAGE_LOCATION_2 = 0x02;
constexpr uint8_t PAGE_IDENTIFICATION = 0x03;
constexpr uint8_t PAGE_BATTERY_STATUS = 0x06;
constexpr uint8_t PAGE_IDENTIFICATION_1 = 0x10;
constexpr uint8_t PAGE_IDENTIFICATION_2 = 0x11;
constexpr uint8_t PAGE_REQUEST = 0x46;


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

namespace ant {
    static DSISerialGeneric *pclSerial = nullptr;
    static DSIFramerANT *pclANT = nullptr;
    static std::set<uint8_t> knownIndexes;
    static std::set<uint8_t> discoveredIndexes;
    static std::set<uint16_t> configuredDevIds;
    static std::map<uint8_t, std::string> partialNames;
    static std::map<uint8_t, int> firstPageCounters;

    enum class AntProfile {
        Unknown,
        AssetTracker,
        HeartRate,
        GarminProprietary
    };

    bool initialize(const UCHAR ucDeviceNumber) {
        printf("ANT initialization started...\n");

        pclSerial = new DSISerialGeneric();
        if (!pclSerial->Init(50000, ucDeviceNumber)) {
            printf("Failed to open USB port %u\n", ucDeviceNumber);
            return false;
        }
        pclANT = new DSIFramerANT(pclSerial);
        pclSerial->SetCallback(pclANT);
        if (!pclANT->Init()) {
            printf("Framer Init failed\n");
            return false;
        }
        if (!pclSerial->Open()) {
            printf("Serial Open failed\n");
            return false;
        }

        pclANT->ResetSystem();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        while (true) {
            const USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);
            if (length > 0 && length != DSI_FRAMER_TIMEDOUT) {
                ANT_MESSAGE msg;
                pclANT->GetMessage(&msg);
                printf("Message ID was %u\n", msg.ucMessageID);
                if (msg.ucMessageID == MESG_STARTUP_MESG_ID) break;
            }
        }
        return true;
    }

    bool startDiscovery() {
        printf("Start ANT discovery...\n");

        if (!pclANT->SetNetworkKey(USER_NETWORK_NUM, USER_NETWORK_KEY, MESSAGE_TIMEOUT)) {
            printf("SetNetworkKey failed\n");
            return false;
        }

        // --- Channel 0: HRM (2457 MHz = ch 57) ---
        if (!pclANT->AssignChannel(USER_CHANNEL_HRM, /*slave=*/0, /*no ack=*/USER_NETWORK_NUM, MESSAGE_TIMEOUT)) {
            printf("AssignChannel failed\n");
            return false;
        }
        if (!pclANT->SetChannelID(USER_CHANNEL_HRM, 0, 0, 0, MESSAGE_TIMEOUT)) {
            printf("SetChannelID failed\n");
            return false;
        }

        // Heart Rate message period
        if (!pclANT->SetChannelPeriod(USER_CHANNEL_HRM, 8070, MESSAGE_TIMEOUT)) {
            printf("SetChannelPeriod failed\n");
            return false;
        }
        if (!pclANT->SetChannelRFFrequency(USER_CHANNEL_HRM, USER_CHANNEL_RF_FREQ, MESSAGE_TIMEOUT)) {
            printf("SetChannelRFFrequency failed\n");
            return false;
        }
        if (!pclANT->OpenChannel(USER_CHANNEL_HRM, MESSAGE_TIMEOUT)) {
            printf("OpenChannel failed\n");
            return false;
        }

        // --- Channel 1: Garmin Asset/GPS (2457 MHz = ch 57), slave=0x10 (unidirectional slave) ---
        if (!pclANT->AssignChannel(USER_CHANNEL_ASSET, /*slave=*/0x10, /*no ack=*/USER_NETWORK_NUM, MESSAGE_TIMEOUT)) {
            printf("AssignChannel failed\n");
            return false;
        }
        if (!pclANT->SetChannelID(USER_CHANNEL_ASSET, 0, 0, 0, MESSAGE_TIMEOUT)) {
            printf("SetChannelID failed\n");
            return false;
        }
        // Asset Tracker message period
        if (!pclANT->SetChannelPeriod(USER_CHANNEL_ASSET, 8091, MESSAGE_TIMEOUT)) {
            printf("SetChannelPeriod failed\n");
            return false;
        }
        if (!pclANT->SetChannelRFFrequency(USER_CHANNEL_ASSET, USER_CHANNEL_RF_FREQ, MESSAGE_TIMEOUT)) {
            printf("SetChannelRFFrequency failed\n");
            return false;
        }
        if (!pclANT->OpenChannel(USER_CHANNEL_ASSET, MESSAGE_TIMEOUT)) {
            printf("OpenChannel failed\n");
            return false;
        }

        pclANT->RxExtMesgsEnable(TRUE);
        printf("Discovery started successfully!\n");
        return true;
    }

    void requestAssetPage(const uint8_t page, const uint8_t index) {
        uint8_t request[8] = {
            PAGE_REQUEST, 0x00, page, 0x00, 0x00, index, 0xFF, 0xFF
        };
        pclANT->SendBroadcastData(USER_CHANNEL_ASSET, request);
    }

    void requestAssetIdentification(const uint8_t index) {
        uint8_t request[8] = {
            PAGE_REQUEST, 0xFF, 0xFF, 0xFF, 0xFF, 0x04, PAGE_IDENTIFICATION_1, 0x04
        };
        const bool ok = pclANT->SendAcknowledgedData(USER_CHANNEL_ASSET, request);
        std::cout << "SendAcknowledgedData returned: " << (ok ? "OK" : "FAIL") << "\n";
        std::cout << "Requested identification (pages 0x10+0x11) from #" << static_cast<int>(index) << "\n";
    }

    void handleAssetIdentification(const uint8_t* data) {
        Device d;
        d.index = data[1] & 0x1F;
        d.icon  = (data[1] & 0xE0) >> 5;
        d.fullName = std::string(reinterpret_cast<const char*>(&data[2]), 8);

        std::cout << "[Asset/3] #" << static_cast<int>(d.index)
                  << " name='" << d.fullName << "' icon=" << static_cast<int>(d.icon) << "\n";
    }

    void handleIdentificationPage1(const uint8_t* data) {
        const uint8_t index = data[1] & 0x1F;
        firstPageCounters[index]++;
        if (firstPageCounters[index] > 3) {
            std::cout << "[Asset] #" << static_cast<int>(index) << " seems disconnected → removing\n";
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

        std::cout << "[Asset/11] #" << static_cast<int>(d.index)
                  << " full name='" << d.fullName << "'\n";
    }

    void handleLocationPage1(const uint8_t* data) {
        Device d;
        d.index = data[1] & 0x1F;
        firstPageCounters[d.index]++;
        if (firstPageCounters[d.index] > 3) {
            std::cout << "[Asset] #" << static_cast<int>(d.index) << " seems disconnected → removing\n";
            knownIndexes.erase(d.index);
            partialNames.erase(d.index);
            firstPageCounters.erase(d.index);
            return;
        }
        if (knownIndexes.insert(d.index).second) {
            requestAssetPage(PAGE_BATTERY_STATUS, d.index);
            requestAssetIdentification(d.index);
        }

        d.distance = data[2] | (data[3] << 8);
        float bearingBradians = data[4] / 256.0f * 2.0f * static_cast<float>(M_PI);
        d.headingDegrees = bearingBradians * (180.0f / static_cast<float>(M_PI));

        const uint8_t status = data[5];
        d.gpsLost    = status & 0x01;
        d.commsLost  = status & 0x02;
        d.remove     = status & 0x04;
        d.lowBattery = status & 0x08;

        std::cout << "[Asset/1] #" << static_cast<int>(d.index)
                  << " → " << d.distance << "m @ "
                  << std::fixed << std::setprecision(1) << d.headingDegrees << "°";

        if (d.gpsLost)     std::cout << " | GPS Lost";
        if (d.commsLost)   std::cout << " | Comms Lost";
        if (d.remove)      std::cout << " | Remove";
        if (d.lowBattery)  std::cout << " | Battery Low";
        std::cout << "\n";
    }

    void handleLocationPage2(const uint8_t* data) {
        Device d;
        d.index = data[1] & 0x1F;
        firstPageCounters[d.index] = 0;
        requestAssetIdentification(d.index);

        const int32_t lat = data[2] | data[3] << 8 | data[4] << 16 | data[5] << 24;
        const int32_t lon = data[6] | data[7] << 8 | data[8] << 16 | data[9] << 24;

        d.latitude  = lat * (180.0 / 0x80000000);
        d.longitude = lon * (180.0 / 0x80000000);

        std::cout << "[Asset/2] #" << static_cast<int>(d.index)
                  << " @ " << std::fixed << std::setprecision(6)
                  << d.latitude << ", " << d.longitude << "\n";
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

        std::cout << "[Asset/6] #" << static_cast<int>(d.index)
                  << " Battery: " << d.batteryLevel << "\n";
    }

    void handleUnknownPage(const uint8_t* data) {
        const uint8_t page = data[0];
        std::cout << "[Asset/?] Unknown page: 0x" << std::hex << static_cast<int>(page) << "\n";
    }

    void onAssetTrackerMessage(const uint8_t* data, const uint8_t length) {
        if (length < 2) return;
        const uint8_t page = data[0];

        switch (page) {
            case PAGE_LOCATION_1:         handleLocationPage1(data); break;
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
            std::cout << "[Ignored] Implausible HR: " << static_cast<int>(hr) << " bpm" << std::endl;
            return;
        }

        std::cout << "Heart Rate: " << static_cast<int>(hr) << " bpm";

        if (length >= 14) {
            const uint16_t devId = d[9] | (d[10] << 8);
            const int8_t rssi = static_cast<int8_t>(d[11]);
            const uint8_t flags = d[12];

            std::cout << " | Device ID: " << devId
                      << " | RSSI: " << static_cast<int>(rssi) << " dBm";

            if (flags & (1 << 4)) std::cout << " | Proximity info present";
            if (flags & (1 << 3)) std::cout << " | RSSI included";
            if (flags & (1 << 2)) std::cout << " | Channel type info";
            if (flags & (1 << 1)) std::cout << " | Transmission type info";
            if (flags & (1 << 0)) std::cout << " | Device type info";

            std::cout << " | Flags: 0x" << std::hex << static_cast<int>(flags) << std::dec;
        }

        std::cout << std::endl;
    }

    void onHeartRateMessage1(const uint8_t* d, const uint8_t length) {
        // Assume standard ANT+ HRM always
        const uint8_t hr = static_cast<int>(d[8]);

        if (hr < 30 || hr > 220) {
            std::cout << "[Ignored] Implausible HR: " << static_cast<int>(hr) << " bpm" << std::endl;
            return;
        }

        std::cout << "Heart Rate: " << int(hr) << " bpm";

        if (length >= 14) {
            const uint16_t devId = d[9] | (d[10] << 8);

            uint8_t txType = 0;
            uint8_t devType = 0;
            int8_t rssi = 0;
            uint8_t flags = 0;

            // safe fallback for minimal ext
            if (length >= 14) {
                constexpr int extOffset = 11;
                rssi = static_cast<int8_t>(d[extOffset]);
                flags = d[extOffset + 1];
            }

            // Decode optional tx/dev type if present
            if (length >= 16) {
                txType  = d[11];
                devType = d[12];
                rssi    = static_cast<int8_t>(d[13]);
                flags   = d[14];
            }

            std::cout << " | Device ID: " << devId;

            if (txType || devType) {
                std::cout << " | TxType: 0x" << std::hex << static_cast<int>(txType)
                          << " | DevType: 0x" << static_cast<int>(devType) << std::dec;

                // Optionally map known dev types:
                if (devType == 0x78)
                    std::cout << " (HRM)";
                else if (devType == 0x7C)
                    std::cout << " (Speed+Cadence)";
                else if (devType == 0x0F)
                    std::cout << " (Garmin GPS)";
                else
                    std::cout << " (Unknown Dev)";
            }

            std::cout << " | RSSI: " << static_cast<int>(rssi) << " dBm";

            if (flags & (1 << 4)) std::cout << " | Proximity info";
            if (flags & (1 << 3)) std::cout << " | RSSI flag";
            if (flags & (1 << 2)) std::cout << " | Channel type";
            if (flags & (1 << 1)) std::cout << " | TxType flag";
            if (flags & (1 << 0)) std::cout << " | DevType flag";

            std::cout << " | Flags: 0x" << std::hex << static_cast<int>(flags) << std::dec;
        }

        std::cout << std::endl;
    }


    void onGenericMessage(const uint8_t* d, const uint8_t length) {
        printf("Generic ANT+ payload: ");
        for (int i = 0; i < length; ++i) {
            printf("%02X ", d[i]);
        }
        printf("\n");

        if (length >= 11) {
            const uint16_t devId = d[9] | (d[10] << 8);
            std::cout << "[Generic] Device ID: " << devId << "\n";
        }

        // Extract and set ChannelID if possible
        if (length >= 13) {
            const uint16_t devId = d[9] | (d[10] << 8);
            const uint8_t txType = d[11];
            const uint8_t devType = d[12];

            std::cout << "[Generic] Device ID: " << devId << "\n";

            if (configuredDevIds.insert(devId).second) {
                std::cout << "[ANT] Setting specific Channel ID → devId=" << devId
                          << " devType=0x" << std::hex << static_cast<int>(devType)
                          << " txType=0x" << static_cast<int>(txType) << std::dec << "\n";

                pclANT->SetChannelID(USER_CHANNEL_ASSET, devId, devType, txType, MESSAGE_TIMEOUT);
            }
        }

        if (length >= 2 && d[0] == 0x00) {
            const uint8_t index = d[1] & 0x1F;
            if (discoveredIndexes.insert(index).second) {
                std::cout << "[Generic] Page 0 received from #" << static_cast<int>(index)
                          << " → requesting identification...\n";
                requestAssetIdentification(index);
            }
        }
    }


    void onGarminProprietary(const uint8_t* d, const uint8_t length) {
        std::cout << "[GarminProprietary] Suspicious packet: ";
        for (int i = 0; i < length; ++i) {
            printf("%02X ", d[i]);
        }
        std::cout << std::endl;
    }

    void dumpHex(const uint8_t* d, const uint8_t length) {
        return;
        std::cout << "Raw payload (" << static_cast<int>(length) << "): ";
        for (int i = 0; i < length; ++i) {
            printf("%02X ", d[i]);
        }
        std::cout << std::endl;
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


    void dispatchAntPlusMessage(const ANT_MESSAGE& msg, const uint8_t length) {
        dumpHex(msg.aucData, length);
        const AntProfile profile = detectProfile(msg, length);
        switch (profile) {
            case AntProfile::HeartRate:
                onHeartRateMessage1(msg.aucData, length);
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
        printf("Starting event loop...\n");
        auto lastMessageTime = std::chrono::steady_clock::now();
        while (searching) {
            auto now = std::chrono::steady_clock::now();
            const USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);

            if (length == DSI_FRAMER_TIMEDOUT || length == 0) {
                const auto secondsSinceLast = std::chrono::duration_cast<std::chrono::seconds>(now - lastMessageTime).count();
                if (secondsSinceLast > 5) {
                    std::cout << "[Info] No ANT messages received in the last " << secondsSinceLast << " seconds.\n";
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
                if (false)
                std::cout << "[ANT] Event or response received: msgID=" << static_cast<int>(msg.ucMessageID)
                          << " | Code=" << static_cast<int>(msg.aucData[2]) << "\n";
                continue;
            }

            printf("Got msgID=%u, len=%u\n", ucMessageID, length);

            if (ucMessageID == MESG_BROADCAST_DATA_ID ||
                ucMessageID == MESG_EXT_BROADCAST_DATA_ID) {

                lastMessageTime = now;
                dispatchAntPlusMessage(msg, length);

            }
        }
    }

    void cleanup() {
        searching = false;
        if (pclANT) {
            std::cout << "Closing ANT channel...\n";
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
