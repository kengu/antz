//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//

#ifndef HRM_DISCOVERY_H
#define HRM_DISCOVERY_H

#include "dsi_framer_ant.hpp"  // for ANT_MESSAGE
#include "profile_discovery.h"

// Must be unique for this profile
constexpr UCHAR HRM_CHANNEL = 0;

// The asset tracker is a master device; therefore,
// the display device must be configured as the slave.
// Bidirectional communication is required.
constexpr UCHAR HRM_CHANNEL_TYPE = 0x00;

// Data is transmitted from the ANT+ heart rate monitor every
// 8070/32768 seconds (4.06 Hz) however the receive rate may
// be set lower if desired.
constexpr USHORT HRM_CHANNEL_PERIOD = 8070;

// 120 (0x78) - indicates search for an ANT+ heart rate monitor.
constexpr UCHAR HRM_DEVICE_TYPE = 0x78;

// The default search timeout is set to 30 seconds in the receiver.
// This timeout is implementation specific and can be set by the
// designer to the appropriate value for the system.
constexpr ULONG HRM_SEARCH_TIMEOUT = 0x30;

class HRMDiscovery final : public ProfileDiscovery {
public:
    explicit HRMDiscovery(DSIFramerANT* ant) : ProfileDiscovery(
        AntProfile::HeartRate, ant,
        HRM_CHANNEL,
        HRM_CHANNEL_TYPE,
        HRM_CHANNEL_PERIOD,
        HRM_DEVICE_TYPE,
        HRM_SEARCH_TIMEOUT
    ) {}

    bool accept(const ANT_MESSAGE& msg, uint8_t length, ExtendedInfo& ext) override;

    void handleMessage(const ANT_MESSAGE& msg, uint8_t length, ExtendedInfo& ext) override;

};

#endif // HRM_DISCOVERY_H
