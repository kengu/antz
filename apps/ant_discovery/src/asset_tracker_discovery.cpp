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
    if (msg.ucMessageID != MESG_BROADCAST_DATA_ID &&
        msg.ucMessageID != MESG_EXT_BROADCAST_DATA_ID) {
        return false;
    }
    if (length < 10) {
        return false;
    }
    const uint8_t page = msg.aucData[0];
    return page == PAGE_LOCATION_1 || page == PAGE_LOCATION_2 ||
           page == PAGE_IDENTIFICATION_1 || page == PAGE_IDENTIFICATION_2 ||
           page == PAGE_NO_ASSETS || page == PAGE_DISCONNECT ||
           page == PAGE_MANUFACTURER_IDENT || page == PAGE_PRODUCT_INFO ||
           page == PAGE_BATTERY_STATUS;
}

// Not implemented. The working Asset Tracker decoder is in discovery.cpp —
// parseDevice(), decodeSituation() and the page handlers — and porting it here
// is the DiscoveryMachine migration, not a gap to be filled in passing.
//
// This body previously held a copy of the HRM decoder: it read byte 8 as a
// heart rate and byte 9-10 as a device id, on pages that carry neither. It
// never ran, because nothing instantiates this class in the built binary, but
// it read as working code and would have been trusted by whoever wired the
// machine up. An explicit refusal is safer than a plausible wrong answer.
void AssetTrackerDiscovery::handleMessage(const ANT_MESSAGE& msg, const uint8_t length, ExtendedInfo& ext) {
    (void)msg;
    (void)length;
    (void)ext;

    static bool warned = false;
    if (!warned) {
        warned = true;
        ant::warn("[Asset] AssetTrackerDiscovery::handleMessage is unimplemented; "
                  "the live decoder is discovery.cpp");
    }
}
