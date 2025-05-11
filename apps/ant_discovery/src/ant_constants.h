//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//

#ifndef ANT_DEVICE_CONSTANTS_H
#define ANT_DEVICE_CONSTANTS_H

#include <cstdint>

// ANT+ Device Types
constexpr uint8_t DEVICE_TYPE_ASSET_TRACKER     = 0x29;

// Transmission Type Wildcard
constexpr uint8_t TRANSMISSION_TYPE_WILDCARD    = 0x00;

// Channel frequency (2457 MHz)
constexpr uint8_t USER_CHANNEL_RF_FREQ          = 57;

// Network Number (usually 0 for public ANT+)
constexpr uint8_t USER_NETWORK_NUM              = 0;

// Timeouts (adjust as needed)
constexpr ULONG MESSAGE_TIMEOUT                 = 1000;

// Rx Timestamp messaging is enabled if message data[12] is 0x20
constexpr uint8_t RX_TIMESTAMP_FLAG             = 0x20;

// RSSI extended messaging is enabled if message data[12] is 0x40
constexpr uint8_t RSSI_EXT_FLAG                 = 0x40;

// Channel ID extended messaging is enabled if message data[12] is 0x80
constexpr uint8_t CHANNEL_ID_EXT_FLAG           = 0x80;

// Default public ANT+ network key (used by all standard ANT+ devices)
// Source: ANT+ Alliance documentation – fixed value for open access
static UCHAR USER_NETWORK_KEY[16] = {
    0xB9, 0xA5, 0x21, 0xFB,
    0xBD, 0x72, 0xC3, 0x45
};
static UCHAR USER_NETWORK_KEY_P[8] = {
    0xB9, 0xA5, 0x21, 0xFB,
    0xBD, 0x72, 0xC3, 0x45
};

// -----------------------------
// --- Required Common Pages ---
// -----------------------------

// Common Page 70: Request Data Page (0x46). Common data page allows an
// ANT+ display device to request a specific data page from an ANT+
// asset tracker device. The Request Data Page shall always be sent as
// an acknowledgement message.
constexpr UCHAR PAGE_REQUEST = 0x46;

// Common Page 80 – Manufacturer’s Identification (0x50) transmits the device’s
// transmit the manufacturer’s ID, model number, and hardware revision. An asset
// tracker shall respond to requests for common page 80, and transmit the page
// once every 1920 messages.
constexpr UCHAR PAGE_MANUFACTURER_IDENT = 0x50;

// Common Page 81 – Product Information (0x51) transmits the device’s
// software revision and its 32-bit serial number. An asset tracker
// shall respond to requests for common page 80, and transmit the page
// once every 1920 messages.
constexpr UCHAR PAGE_PRODUCT_INFO = 0x51;

// Common Page 82 – Battery Status (0x52) transmits the device’s
// battery voltage and status. An asset tracker shall respond
// to requests for common page 80, and transmit the page
// once every 1920 messages.
constexpr UCHAR PAGE_BATTERY_STATUS = 0x52;



#endif //ANT_DEVICE_CONSTANTS_H
