//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//

#ifndef ASSET_TRACKER_DISCOVERY_H
#define ASSET_TRACKER_DISCOVERY_H

#include "dsi_framer_ant.hpp"  // for ANT_MESSAGE
#include "profile_discovery.h"

// Must be unique for this profile
constexpr UCHAR ASSET_TRACKER_CHANNEL         = 1;

// The asset tracker is a master device; therefore,
// the display device must be configured as the slave.
// Bidirectional communication is required.
constexpr UCHAR ASSET_TRACKER_CHANNEL_TYPE    = 0x00;

// Data is transmitted from the Asset Tracker every
// 2048/32768 seconds (16 Hz) and must be received at this rate.
constexpr USHORT ASSET_TRACKER_CHANNEL_PERIOD  = 2048;

// 41 (0x29) – indicates search for an ANT+ asset tracker.
constexpr UCHAR ASSET_TRACKER_DEVICE_TYPE     = 0x29;

// The default search timeout is set to 7.5 seconds in the receiver.
// This timeout is implementation specific and can be set by the designer
// to the appropriate value for the system.
constexpr ULONG ASSET_TRACKER_SEARCH_TIMEOUT  = 0x07;

// ---------------------------
// --- 7.4 Main Data Pages ---
// ---------------------------

// Data page 1:- Asset Location Page 1 (0x01) is sent as a broadcast message,
// and is immediately followed by an Asset Location Page 2 for the same asset.
constexpr UCHAR PAGE_LOCATION_1 = 0x01;

// Data page 2: Asset Location Page 2 (0x02) is sent as a broadcast message,
// and is immediately preceded by an Asset Location Page 1 for the same asset.
constexpr UCHAR PAGE_LOCATION_2 = 0x02;

// Data Page 3: No Assets (0x03) is sent as the main page when no assets
// are connected to the asset tracker.
constexpr UCHAR PAGE_NO_ASSETS = 0x03;

// Data Page 4 – 15 are reserved for future main data page definitions

// ----------------------------------------
// --- 7.6 Asset Identifying Data Pages ---
// ----------------------------------------

// Data page 16: Data Page 16 – Asset Identification Page 1 (0x10)
// is sent as a broadcast message, and is immediately followed by an
// Asset Identification Page 2 for the same asset
constexpr UCHAR PAGE_IDENTIFICATION_1 = 0x10;

// Data page 17: Data Page 16 – Asset Identification Page 2 (0x11)
// is sent as a broadcast message, and is immediately preceded by an
// Asset Identification Page 1 for the same asset.
constexpr UCHAR PAGE_IDENTIFICATION_2 = 0x11;

// Data Page 18 – 31 are reserved for future main data page definitions.

// -------------------------
// --- 7.8 Command Pages ---
// -------------------------

// Data Page 32: Disconnect Command (0x20). The disconnect command
// is sent as a broadcast message 5 times before an asset tracker turns off.
// A display device should, on receiving this page, close its slave channel
// and report the disconnection to the user
constexpr UCHAR PAGE_DISCONNECT = 0x20;

// Data Page 33 – 63 are reserved for future main data page definitions.

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

// Data Page Number = 70 (0x46).

class AssetTrackerDiscovery final : public ProfileDiscovery {
public:
    explicit AssetTrackerDiscovery(DSIFramerANT *ant) : ProfileDiscovery(
        AntProfile::HeartRate, ant,
        ASSET_TRACKER_CHANNEL,
        ASSET_TRACKER_CHANNEL_TYPE,
        ASSET_TRACKER_CHANNEL_PERIOD,
        ASSET_TRACKER_DEVICE_TYPE,
        ASSET_TRACKER_SEARCH_TIMEOUT
    ) {}

    bool accept(const ANT_MESSAGE& msg, uint8_t length, ExtendedInfo& ext) override;

    void handleMessage(const ANT_MESSAGE& msg, uint8_t length, ExtendedInfo& ext) override;

};

#endif // ASSET_TRACKER_DISCOVERY_H
