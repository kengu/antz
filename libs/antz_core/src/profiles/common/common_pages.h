//
// ANT+ Common Data Pages (D00001198) — the two that carry identity, and the
// transmitting device's own battery.
//
// Common to every ANT+ device profile rather than to one, which is why they
// sit beside the profiles instead of inside `tracker/`. A heart-rate strap
// and a Garmin Alpha answer Page 80 and Page 81 the same way.
//
// Both are required transmissions and both are requestable, so a display can
// ask for them rather than waiting out a broadcast rotation. That matters for
// identity: the pair is what names a device to something outside ANT+, and
// the Streamdog platform's ADR 0070 uses `"<manufacturer_id>:<serial>"` from
// exactly these two pages as a handheld's foreign identity. The 32-bit serial
// survives battery changes and re-pairings where the u16 ANT device number
// does not.
//
// Held to packages/platform-messages/spec/wire/ant/vectors/ in the
// streamdog-platform repository.
//

#pragma once

#include <stdint.h>

typedef enum {
    ANTZ_COMMON_PAGE_MANUFACTURER = 0x50, // Page 80, manufacturer identification
    ANTZ_COMMON_PAGE_PRODUCT      = 0x51, // Page 81, product information
    ANTZ_COMMON_PAGE_BATTERY      = 0x52, // Page 82, battery status
} antz_common_page_e;

// Page 82's battery status, bits 4-6 of byte 7. 0 and 6 are reserved.
typedef enum {
    ANTZ_BATTERY_STATUS_NEW      = 1,
    ANTZ_BATTERY_STATUS_GOOD     = 2,
    ANTZ_BATTERY_STATUS_OK       = 3,
    ANTZ_BATTERY_STATUS_LOW      = 4,
    ANTZ_BATTERY_STATUS_CRITICAL = 5,
    ANTZ_BATTERY_STATUS_INVALID  = 7,
} antz_battery_status_e;

// Page 80. The registry triple off the air: ANT+ manufacturer id, the
// maker's own model number, and a hardware revision.
typedef struct {
    uint8_t  hw_revision;
    uint16_t manufacturer_id;
    uint16_t model_number;
} antz_common_manufacturer_t;

// Page 81.
//
// `serial_valid` is false for the all-ones sentinel, which means the device
// declines to give one. Zero is not that sentinel and is not a refusal, so a
// caller must read the flag rather than test the value — a serial read as 0
// would make every such device the same device.
//
// `sw_revision_supplemental_valid` is false for 0xFF, which is the same
// arrangement one field down: a device with no supplemental revision reports
// only the main one, and 255 is not a revision number.
typedef struct {
    uint8_t  sw_revision_main;
    uint8_t  sw_revision_supplemental;
    bool     sw_revision_supplemental_valid;
    uint32_t serial;
    bool     serial_valid;
} antz_common_product_t;

// Page 82. The battery of the device transmitting it — like Pages 80 and 81,
// a common page describes its sender. On a Tracker's channel that is the
// handheld, not a collar: a collar's battery is the low-battery bit in its
// asset's Page 1. Checked on hardware: an Astro 320 on two AA cells reads
// 2.59 V and an Alpha 10 on a lithium cell 3.77 V.
//
// Each measurement carries its own validity, because a device may give one
// without the other: `voltage_valid` is false for coarse volts 0x0F, and
// `status_valid` false for a reserved or invalid status. A caller reads the
// flag, never the value — 0x0F would otherwise be fifteen volts.
typedef struct {
    // Byte 2: bits 0-3 how many batteries, bits 4-7 which one this is.
    // All-ones means the device reports one battery and does not number it.
    bool     identifier_valid;
    uint8_t  battery_count;
    uint8_t  battery_identifier;
    // Bytes 3-5, cumulative operating time: the 24-bit count as sent, the
    // tick byte 7's bit 7 selects (2 s when set, 16 s when clear), and their
    // product. The count rolls over; a caller comparing two readings must
    // allow for it.
    uint32_t operating_time_ticks;
    uint8_t  operating_time_resolution_s;
    uint32_t operating_time_s;
    // Byte 7 bits 0-3 whole volts and byte 6 in 1/256 V, as sent, and the
    // voltage they make. The fraction means nothing without the whole volts,
    // so `voltage_v` is 0 when `voltage_valid` is false.
    uint8_t  voltage_coarse;
    uint8_t  voltage_fractional;
    bool     voltage_valid;
    float    voltage_v;
    // Byte 7 bits 4-6, as sent: an antz_battery_status_e when status_valid.
    uint8_t  status;
    bool     status_valid;
} antz_common_battery_t;
