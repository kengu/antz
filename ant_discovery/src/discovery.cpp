#include "types.h"
#include "dsi_framer_ant.hpp"
#include "dsi_thread.h"
#include "dsi_serial_generic.hpp"
#include "ant.h"

#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>

constexpr UCHAR USER_ANTCHANNEL_HRM = 0;
constexpr UCHAR USER_ANTCHANNEL_ASSET = 1;
constexpr UCHAR USER_NETWORK_NUM = 0;
constexpr ULONG MESSAGE_TIMEOUT = 1000;
static UCHAR USER_NETWORK_KEY[8] = {
    0xB9, 0xA5, 0x21, 0xFB,
    0xBD, 0x72, 0xC3, 0x45
};

namespace ant {
    static DSISerialGeneric *pclSerial = nullptr;
    static DSIFramerANT *pclANT = nullptr;

    bool initialize(UCHAR ucDeviceNumber) {
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
            USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);
            if (length > 0 && length != DSI_FRAMER_TIMEDOUT) {
                ANT_MESSAGE msg;
                pclANT->GetMessage(&msg);
                printf("Message ID was %u\n", msg.ucMessageID);
                if (msg.ucMessageID == MESG_STARTUP_MESG_ID) break;
            }
        }
        return true;
    }

    // **Ny overload**: ingen argumenter → bruker port 0 som default
    bool initialize() {
        return initialize(static_cast<UCHAR>(0));
    }

    bool startDiscovery() {
        printf("Start ANT discovery...\n");

        if (!pclANT->SetNetworkKey(USER_NETWORK_NUM, USER_NETWORK_KEY, MESSAGE_TIMEOUT)) {
            printf("SetNetworkKey failed\n");
            return false;
        }

        // --- Kanal 0: HRM (2457 MHz = ch 57) ---
        if (!pclANT->AssignChannel(USER_ANTCHANNEL_HRM, /*slave=*/0, /*no ack=*/0, MESSAGE_TIMEOUT)) {
            printf("AssignChannel failed\n");
            return false;
        }
        if (!pclANT->SetChannelID(USER_ANTCHANNEL_HRM, 0, 0, 0, MESSAGE_TIMEOUT)) {
            printf("SetChannelID failed\n");
            return false;
        }

        // Heart-Rate-meldingperiode
        if (!pclANT->SetChannelPeriod(USER_ANTCHANNEL_HRM, 8070, MESSAGE_TIMEOUT)) {
            printf("SetChannelPeriod failed\n");
            return false;
        }
        if (!pclANT->SetChannelRFFrequency(USER_ANTCHANNEL_HRM, 57, MESSAGE_TIMEOUT)) {
            printf("SetChannelRFFrequency failed\n");
            return false;
        }
        if (!pclANT->OpenChannel(USER_ANTCHANNEL_HRM, MESSAGE_TIMEOUT)) {
            printf("OpenChannel failed\n");
            return false;
        }

        // --- Kanal 1: Garmin Asset/GPS (2466 MHz = ch 66) ---
        if (!pclANT->AssignChannel(USER_ANTCHANNEL_ASSET, /*slave=*/0, /*no ack=*/0, MESSAGE_TIMEOUT)) {
            printf("AssignChannel failed\n");
            return false;
        }
        if (!pclANT->SetChannelID(USER_ANTCHANNEL_ASSET, 0, 0, 0, MESSAGE_TIMEOUT)) {
            printf("SetChannelID failed\n");
            return false;
        }
        if (!pclANT->SetChannelRFFrequency(USER_ANTCHANNEL_ASSET, 66, MESSAGE_TIMEOUT)) {
            printf("SetChannelRFFrequency failed\n");
            return false;
        }
        if (!pclANT->OpenChannel(USER_ANTCHANNEL_ASSET, MESSAGE_TIMEOUT)) {
            printf("OpenChannel failed\n");
            return false;
        }

        pclANT->RxExtMesgsEnable(TRUE);
        printf("Discovery started successfully!\n");
        return true;
    }

    void handleAssetIdentification(const uint8_t* d) {
        uint8_t idx = d[1];
        char fragment[7] = {0};
        memcpy(fragment, &d[2], 6);
        printf("Asset %u name fragment: \"%s\"\n", idx, fragment);
    }

    void handleAssetLocation(const uint8_t* d) {
        uint8_t idx = d[1];
        uint32_t lat = d[2] | (d[3] << 8) | (d[4] << 16) | (d[5] << 24);
        uint32_t lon = d[6] | (d[7] << 8) | (d[8] << 16) | (d[9] << 24);
        double latitude = lat * (180.0 / 0x80000000);
        double longitude = lon * (180.0 / 0x80000000);
        printf("Asset %u @ %.6f, %.6f\n", idx, latitude, longitude);
    }

    void handleBatteryStatus(const uint8_t* d) {
        uint8_t idx = d[1];
        uint8_t pct = d[2];
        bool low = d[3] != 0;
        printf("Asset %u battery: %u%% %s\n", idx, pct, low ? "(LOW!)" : "");
    }

    void handleUnknown(const uint8_t* d, uint8_t length) {
        printf("Unknown page %02X, payload: ", d[0]);
        for (int i = 0; i < length; ++i)
            printf("%02X ", d[i]);
        printf("\n");
    }

    void handleGarminAssetTrackerPage0(const uint8_t* d) {
        uint8_t assetIndex = d[1];
        uint32_t latRaw = d[2] | (d[3] << 8) | (d[4] << 16) | (d[5] << 24);
        uint32_t lonRaw = d[6] | (d[7] << 8) | (d[8] << 16) | (d[9] << 24);

        double lat = latRaw * (180.0 / 0x80000000);
        double lon = lonRaw * (180.0 / 0x80000000);

        std::cout << "[Asset] Index " << int(assetIndex)
                  << " @ " << lat << ", " << lon << std::endl;
    }

    void onAssetTrackerMessage(const uint8_t* data, uint8_t length) {
        uint8_t page = data[0];
        switch (page) {
            case 0x02: handleAssetIdentification(data); break;
            case 0x03: handleAssetLocation(data); break;
            case 0x06: handleBatteryStatus(data); break;
            default:   handleUnknown(data, length); break;
        }
    }

    bool isLikelyAssetTracker(const ANT_MESSAGE& msg, uint8_t length) {
        if (msg.ucMessageID != MESG_BROADCAST_DATA_ID &&
            msg.ucMessageID != MESG_EXT_BROADCAST_DATA_ID) {
            return false;
            }

        if (length < 2) return false;

        uint8_t page = msg.aucData[0];
        return (page >= 0x01 && page <= 0x07);
    }

    enum class AntProfile {
        Unknown,
        AssetTracker,
        HeartRate,
        GarminProprietary
    };

    AntProfile detectProfile(const ANT_MESSAGE& msg, uint8_t length) {
        if (msg.ucMessageID != MESG_BROADCAST_DATA_ID &&
            msg.ucMessageID != MESG_EXT_BROADCAST_DATA_ID)
            return AntProfile::Unknown;

        const uint8_t* d = msg.aucData;

        // Asset tracker (Page 0-style)
        if (length == 14 && d[0] == 0x00 && d[5] == 0x00 && d[9] == 0x80) {
            return AntProfile::AssetTracker;
        }

        // Suspicious "Garmin" page 0 messages (junk patterns)
        if (length == 14 && d[0] == 0x00 && d[8] == 0xFF &&
            d[5] == 0xFF && d[6] == 0xFF) {
            return AntProfile::GarminProprietary;
            }

        // Valid HRM packet
        if (length >= 9 && d[0] == 0x00 && d[8] >= 30 && d[8] <= 220) {
            return AntProfile::HeartRate;
        }

        return AntProfile::Unknown;
    }

    void dumpHex(const uint8_t* d, uint8_t length) {
        std::cout << "Raw payload (" << int(length) << "): ";
        for (int i = 0; i < length; ++i) {
            printf("%02X ", d[i]);
        }
        std::cout << std::endl;
    }

    void onHeartRateMessage(const uint8_t* d, uint8_t length) {
        uint8_t hr = 0;

        // Assume standard ANT+ HRM always
        hr = int(d[8]);

        //uint8_t hr = d[8];

        if (hr < 30 || hr > 220) {
            std::cout << "[Ignored] Implausible HR: " << int(hr) << " bpm" << std::endl;
            return;
        }

        std::cout << "Heart Rate: " << int(hr) << " bpm";

        if (length >= 14) {
            uint16_t devId = d[9] | (d[10] << 8);
            int8_t rssi = static_cast<int8_t>(d[11]);
            uint8_t flags = d[12];

            std::cout << " | Device ID: " << devId
                      << " | RSSI: " << int(rssi) << " dBm";

            if (flags & (1 << 4)) std::cout << " | Proximity info present";
            if (flags & (1 << 3)) std::cout << " | RSSI included";
            if (flags & (1 << 2)) std::cout << " | Channel type info";
            if (flags & (1 << 1)) std::cout << " | Transmission type info";
            if (flags & (1 << 0)) std::cout << " | Device type info";

            std::cout << " | Flags: 0x" << std::hex << int(flags) << std::dec;
        }

        std::cout << std::endl;
    }

    void onHeartRateMessage1(const uint8_t* d, uint8_t length) {
        // Assume standard ANT+ HRM always
        uint8_t hr = int(d[8]);

        //uint8_t hr = d[8];

        if (hr < 30 || hr > 220) {
            std::cout << "[Ignored] Implausible HR: " << int(hr) << " bpm" << std::endl;
            return;
        }

        std::cout << "Heart Rate: " << int(hr) << " bpm";

        if (length >= 14) {
            uint16_t devId = d[9] | (d[10] << 8);
            int extOffset = 11;

            uint8_t txType = 0;
            uint8_t devType = 0;
            int8_t rssi = 0;
            uint8_t flags = 0;

            // safe fallback for minimal ext
            if (length >= 14) {
                rssi = static_cast<int8_t>(d[extOffset]);
                flags = d[extOffset + 1];
            }

            // Decode optional tx/dev type if present
            if (length >= 16) {
                txType = d[11];
                devType = d[12];
                rssi    = static_cast<int8_t>(d[13]);
                flags   = d[14];
            }

            std::cout << " | Device ID: " << devId;

            if (txType || devType) {
                std::cout << " | TxType: 0x" << std::hex << int(txType)
                          << " | DevType: 0x" << int(devType) << std::dec;

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

            std::cout << " | RSSI: " << int(rssi) << " dBm";

            if (flags & (1 << 4)) std::cout << " | Proximity info";
            if (flags & (1 << 3)) std::cout << " | RSSI flag";
            if (flags & (1 << 2)) std::cout << " | Channel type";
            if (flags & (1 << 1)) std::cout << " | TxType flag";
            if (flags & (1 << 0)) std::cout << " | DevType flag";

            std::cout << " | Flags: 0x" << std::hex << int(flags) << std::dec;
        }

        std::cout << std::endl;
    }


    void onGenericMessage(const uint8_t* d, uint8_t length) {
        printf("Generic ANT+ payload: ");
        for (int i = 0; i < length; ++i)
            printf("%02X ", d[i]);
        printf("\n");
    }

    void onGarminProprietary(const uint8_t* d, uint8_t length) {
        std::cout << "[GarminProprietary] Suspicious packet: ";
        for (int i = 0; i < length; ++i) {
            printf("%02X ", d[i]);
        }
        std::cout << std::endl;
    }

    void dispatchAntPlusMessage(const ANT_MESSAGE& msg, uint8_t length) {
        //std::cout << "Received on channel " << int(msg.ucANTChannel) << ": ";
        dumpHex(msg.aucData, length);
        AntProfile profile = detectProfile(msg, length);
        switch (profile) {
            case AntProfile::HeartRate:
                onHeartRateMessage1(msg.aucData, length);
                break;
            case AntProfile::AssetTracker:
                handleGarminAssetTrackerPage0(msg.aucData);
                break;
            case AntProfile::GarminProprietary:
                onGarminProprietary(msg.aucData, length);
                break;
            default:
                onGenericMessage(msg.aucData, length);
                break;
        }
    }

    void runEventLoop() {
        printf("Starting event loop...\n");
        while (true) {
            USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);

            if (length == DSI_FRAMER_TIMEDOUT || length == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            ANT_MESSAGE msg;
            pclANT->GetMessage(&msg);

            UCHAR ucMessageID = msg.ucMessageID;

            if (ucMessageID == 0 ||
                ucMessageID == DSI_FRAMER_TIMEDOUT ||
                ucMessageID == MESG_EVENT_ID || // 1: channel event
                ucMessageID == MESG_RESPONSE_EVENT_ID) // 64: ACK/response
            {
                continue;
            }

            printf("Got msgID=%u, len=%u\n", ucMessageID, length);

            if (ucMessageID == MESG_BROADCAST_DATA_ID ||
                ucMessageID == MESG_EXT_BROADCAST_DATA_ID) {

                dispatchAntPlusMessage(msg, length);

            }
        }
    }

    void cleanup() {
        if (pclANT) {
            std::cout << "Closing ANT channel...\n";
            pclANT->CloseChannel(USER_ANTCHANNEL_HRM);
            pclANT->UnAssignChannel(USER_ANTCHANNEL_HRM);
            pclANT->CloseChannel(USER_ANTCHANNEL_ASSET);
            pclANT->UnAssignChannel(USER_ANTCHANNEL_ASSET);
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
