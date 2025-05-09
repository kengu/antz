//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//

#include "dsi_framer_ant.hpp"

#include <string>

#include "hrm_discovery.h"

#include "ant_device.h"
#include "logging.h"

bool HRMDiscovery::accept(const ANT_MESSAGE& msg, const uint8_t length, ExtendedInfo& ext) {
    if (msg.ucMessageID == MESG_BROADCAST_DATA_ID ||
        msg.ucMessageID == MESG_EXT_BROADCAST_DATA_ID) {

        const uint8_t* d = msg.aucData;

        if (length >= 14) {
            // Detect HRM only if DevType == 0x78
            if (d[0] == 0x00 && ext.dType == deviceType_) {
                return true;
            }
        }
    }
    return false;
}

// Handle the incoming message for the HRM profile
void HRMDiscovery::handleMessage(const ANT_MESSAGE& msg, const uint8_t length, ExtendedInfo& ext) {
    const uint8_t* d = msg.aucData;

    // Assume standard ANT+ HRM always
    const uint8_t hr = static_cast<int>(d[8]);

    if (hr < 30 || hr > 220) {
        std::ostringstream oss;
        oss << "[HRM] (Ignored) Implausible HR: " << static_cast<int>(hr) << " bpm";
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
                devType = ext.dType;
                txType = ext.tType;
            }
            oss << " | Trailer bytes used: " << static_cast<int>(ext.length);

            oss << " | "<< formatDeviceInfo(devId, devType, txType);

            if (ext.hasRssi) oss << " | RSSI: " << static_cast<int>(ext.rssi) << " dBm";
            if (ext.hasProximity) oss << " | Proximity: " << static_cast<int>(ext.threshold);

            oss << " | Flags: 0x" << std::hex << static_cast<int>(flags) << std::dec;

            info(oss.str());
        }
    }
}