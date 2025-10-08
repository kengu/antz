//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//


#include <string>

#include "dsi_framer_ant.hpp"
#include "asset_tracker_discovery.h"
#include "ant_device.h"
#include "logging.h"

// -------------------------------------------------------------------------
// Pairing validator for ANT+ Asset Tracker (Device Type 0x29)
//
// Based on Device Profile Rev 1.0, Chapter 6 – Device Pairing.
//
// Requirements:
// - Page number must be 0x01 (Location Page 1)
// - Device Type in trailer must be 0x29 (Asset Tracker)
// - Device ID must be non-zero
//
// Optional fields (distance, bearing, etc) are *not* required to be valid
// during pairing – but may affect user confidence or be used for logger.
// -------------------------------------------------------------------------

bool AssetTrackerDiscovery::accept(const ANT_MESSAGE& msg, const uint8_t length, ExtendedInfo& ext) {
    if (msg.ucMessageID == MESG_BROADCAST_DATA_ID ||
        msg.ucMessageID == MESG_EXT_BROADCAST_DATA_ID) {

        const uint8_t* d = msg.aucData;

        if (length >= 10) {
            // Detect Asset Tracker pages
            if (d[0] == PAGE_LOCATION_1 || d[0] == PAGE_LOCATION_2 ||
                d[0] == PAGE_IDENTIFICATION_1 || d[0] == PAGE_IDENTIFICATION_2 ||
                d[0] == PAGE_NO_ASSETS || d[0] == PAGE_DISCONNECT || d[0] == PAGE_MANUFACTURER_IDENT ||
                d[0] == PAGE_PRODUCT_INFO || d[0] == PAGE_BATTERY_STATUS) {
                return true;
            }
        }
    }
    return false;
}

// Handle the incoming message for the AssetTracker profile
void AssetTrackerDiscovery::handleMessage(const ANT_MESSAGE& msg, const uint8_t length, ExtendedInfo& ext) {
    const uint8_t* d = msg.aucData;

    // Assume standard ANT+ AssetTracker always
    const uint8_t hr = static_cast<int>(d[8]);

    if (hr < 30 || hr > 220) {
        std::ostringstream oss;
        oss << "[Asset] (Ignored) Implausible HR: " << static_cast<int>(hr) << " bpm";
        info(oss.str());
        return;
    }

    {
        std::ostringstream oss;
        oss << "Heart Rate: " << static_cast<int>(hr) << " bpm";

        if (length >= 11) {
            const uint16_t devId = d[9] | (d[10] << 8);
            uint8_t flags = 0;

            oss << " | Trailer bytes used: " << static_cast<int>(ext.length);
            oss << " | "<< formatDeviceInfo(devId, ext.dType, ext.tType);
            if (ext.hasRssi) oss << " | RSSI: " << static_cast<int>(ext.rssi) << " dBm";
            if (ext.hasProximity) oss << " | Proximity: " << static_cast<int>(ext.threshold);
            oss << " | Flags: 0x" << std::hex << static_cast<int>(flags) << std::dec;

            info(oss.str());
        }
    }
}