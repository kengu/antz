//
// ANT+ Asset Tracker Device Profile Rev 1.0 (D00001671) page structures.
//
// Deprecated by ANT+ and frozen, which is what makes it safe to encode here:
// the layouts cannot change under us. Four errata in the Rev 1.0 document are
// recorded in the Streamdog platform's ADR 0070; the two that touch these
// structs are that the Asset Location status byte carries `situation` in bits
// 0:2 rather than 5:7, and that the asset index is bits 0:4 of byte 1 with the
// top three set to 0x7.
//
// Held to packages/platform-messages/spec/wire/ant/vectors/ in the
// streamdog-platform repository.
//

#pragma once

#include <stdint.h>

typedef enum {
    ANTZ_TRACKER_PAGE_LOCATION_1     = 0x01, // Asset Location 1
    ANTZ_TRACKER_PAGE_LOCATION_2     = 0x02, // Asset Location 2
    ANTZ_TRACKER_PAGE_NO_ASSETS      = 0x03, // Connected, empty roster
    // The handheld's own position, one whole coordinate per page.
    //
    // TRK Rev 1.0 §7.5 calls 0x04-0x0F "Reserved for Future Use" and a real
    // Garmin Astro transmits these steadily anyway — about a ninth of the
    // asset location rate, always as a pair. Not the Garmin Ranging profile
    // either (D0001697 uses 0x10/0x30/0x31/0xF0).
    //
    // Worth having because the profile gives a display every asset's position
    // and never the tracker's, while asset distance and bearing are measured
    // *from* the tracker. Without these, a Gateway can say where every dog is
    // and not where the hunter is.
    ANTZ_TRACKER_PAGE_SELF_LATITUDE  = 0x04,
    ANTZ_TRACKER_PAGE_SELF_LONGITUDE = 0x05,
    ANTZ_TRACKER_PAGE_IDENTIFICATION_1 = 0x10, // Colour and first name bytes
    ANTZ_TRACKER_PAGE_IDENTIFICATION_2 = 0x11, // Asset type and rest of name
    ANTZ_TRACKER_PAGE_DISCONNECT     = 0x20, // Channel closing
} antz_tracker_page_e;

// TRK Table 7-5. The situation an asset is in, as the handheld assessed it.
typedef enum {
    ANTZ_TRACKER_SITUATION_SITTING  = 0,
    ANTZ_TRACKER_SITUATION_MOVING   = 1,
    ANTZ_TRACKER_SITUATION_POINTED = 2,
    ANTZ_TRACKER_SITUATION_TREED    = 3,
    ANTZ_TRACKER_SITUATION_UNKNOWN  = 4,
    // 5-7 fall inside the 3-bit field and name nothing, so they decode to
    // UNDEFINED alongside the whole-byte 0xFF sentinel. Distinct from
    // UNKNOWN, which is a real assessment the handheld made.
    ANTZ_TRACKER_SITUATION_UNDEFINED = 0xFF,
} antz_tracker_situation_e;

// TRK Table 7-4, the Asset Location 1 status byte.
//
// `situation_valid` is false when the whole byte is 0xFF, which Table 7-5
// gives an Asset Tracker asset — one that has no situation at all. The flags
// are not readable from an all-ones byte either, so none of them means
// anything when this is false.
typedef struct {
    uint8_t                  raw;
    bool                     situation_valid;
    antz_tracker_situation_e situation;
    bool                     low_battery;
    bool                     gps_lost;
    bool                     comms_lost;
    bool                     remove;
} antz_tracker_status_t;

// Page 1. Carries the low half of the latitude; Page 2 carries the high half,
// so neither page is a position on its own.
typedef struct {
    uint8_t               asset_index;
    uint16_t              distance_m;
    uint8_t               bearing_bradians;
    antz_tracker_status_t status;
    uint16_t              latitude_bits_0_15;
} antz_tracker_location1_t;

// Page 2. The high half of the latitude and the whole of the longitude.
typedef struct {
    uint8_t  asset_index;
    uint16_t latitude_bits_16_31;
    int32_t  longitude_semicircles;
} antz_tracker_location2_t;

// Page 16. Colour and the first five characters of the name.
typedef struct {
    uint8_t asset_index;
    uint8_t colour;
    // Not NUL-terminated on the wire; padded with 0x00 when shorter.
    char    name[6];
} antz_tracker_identification1_t;

// TRK Table 7-9. Asset Type 0 is another hunter's tracker seen second-hand,
// 1 is a dog. The distinction matters: filing a person's position under an
// animal is the failure this field prevents.
typedef struct {
    uint8_t asset_index;
    uint8_t asset_type;
    char    name[6];
} antz_tracker_identification2_t;

//
// The handheld's own latitude or longitude (pages 0x04 and 0x05).
//
// Deliberately NOT named location1/location2. Those split one coordinate
// across two pages — Page 1 carries bits 0:15 and Page 2 bits 16:31 — and
// these do not: each carries a whole signed semicircle value. Naming them
// alike would invite exactly the reassembly that is wrong here.
//
// Undocumented, so what is known is separated from what is not. The
// coordinate is established: two payloads off a stationary Astro decoded to
// 9.8 m from the phone lying beside it, which is not a coincidence at that
// precision. `reserved_1` and `reserved_23` are not: they were 0x00 and
// 0xFFFF in both samples and are carried out raw rather than assumed.
//
typedef struct {
    int32_t  semicircles;   // Signed. Unsigned reads land in the wrong hemisphere.
    uint8_t  reserved_1;    // Byte 1. Only 0x00 observed; not known to be constant.
    uint16_t reserved_23;   // Bytes 2-3. Only 0xFFFF observed.
} antz_tracker_self_position_t;
