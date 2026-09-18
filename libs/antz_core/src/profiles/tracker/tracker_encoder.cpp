#include "tracker_encoder.h"

#include <cstring>

#include "data/antz_bytes.h"

namespace antz {

    namespace {
        // The same constants the decoder reads by. Shared deliberately: an
        // encoder with its own copy is two readings of one table, which is
        // the failure this library exists to hold to one.
        constexpr uint8_t ASSET_INDEX_MASK = 0x1F;

        // TRK Table 7-3: bits 5:7 of byte 1 are reserved and set. A page with
        // them clear decodes correctly here and is not what hardware sends.
        constexpr uint8_t ASSET_INDEX_RESERVED = 0xE0;

        constexpr uint8_t SITUATION_MASK   = 0x07;
        constexpr uint8_t FLAG_LOW_BATTERY = 0x08;
        constexpr uint8_t FLAG_GPS_LOST    = 0x10;
        constexpr uint8_t FLAG_COMMS_LOST  = 0x20;
        constexpr uint8_t FLAG_REMOVE      = 0x40;

        constexpr uint8_t STATUS_UNDEFINED = 0xFF;

        // Reserved bytes are 0xFF, not zero. Pages 3 and 32 are all-ones
        // after the page number, and zeroes there would be a page no handheld
        // produces.
        constexpr uint8_t RESERVED_BYTE = 0xFF;

        constexpr uint8_t PAGE_LEN = 8;

        bool index_fits(const uint8_t asset_index) {
            return (asset_index & ~ASSET_INDEX_MASK) == 0;
        }

        uint8_t index_byte(const uint8_t asset_index) {
            return static_cast<uint8_t>(ASSET_INDEX_RESERVED |
                                        (asset_index & ASSET_INDEX_MASK));
        }

        // Names are 0x00-padded on the wire rather than terminated, so a name
        // of exactly five characters fills the field with no terminator. One
        // longer does not fit, and silently truncating it would file a dog
        // under a name nobody chose.
        int write_name(uint8_t* dst, const char* name, const size_t n) {
            const size_t len = strnlen(name, n + 1);
            if (len > n) return -1;
            memset(dst, 0x00, n);
            memcpy(dst, name, len);
            return 0;
        }
    }

    uint8_t tracker_encode_status(const antz_tracker_status_t* status) {
        if (!status) return STATUS_UNDEFINED;

        // The sentinel swallows everything beside it, which is what it means.
        if (!status->situation_valid) return STATUS_UNDEFINED;

        uint8_t out = 0;
        if (status->situation == ANTZ_TRACKER_SITUATION_UNDEFINED) {
            // The decoder yields UNDEFINED for 5, 6 and 7 alike, so there is
            // nothing to write back that distinguishes them. 5 is the lowest,
            // and it decodes to UNDEFINED again.
            out = 5;
        } else {
            out = static_cast<uint8_t>(status->situation) & SITUATION_MASK;
        }

        if (status->low_battery) out |= FLAG_LOW_BATTERY;
        if (status->gps_lost)    out |= FLAG_GPS_LOST;
        if (status->comms_lost)  out |= FLAG_COMMS_LOST;
        if (status->remove)      out |= FLAG_REMOVE;

        // An encoded byte that lands on the sentinel would decode as "no
        // situation at all" and lose every flag beside it. It cannot be
        // reached — the situation field is at most 7 and the four flags make
        // 0x47 — but the assertion is cheap and the failure would be silent.
        return out == STATUS_UNDEFINED ? static_cast<uint8_t>(out & 0x7F) : out;
    }

    int tracker_encode_location1(const antz_tracker_location1_t* in,
                                 uint8_t* out, const uint8_t len) {
        if (!in || !out || len < PAGE_LEN) return -1;
        if (!index_fits(in->asset_index)) return -1;

        out[0] = ANTZ_TRACKER_PAGE_LOCATION_1;
        out[1] = index_byte(in->asset_index);
        uint16_to_bytes(in->distance_m, &out[2]);
        out[4] = in->bearing_bradians;
        out[5] = tracker_encode_status(&in->status);
        uint16_to_bytes(in->latitude_bits_0_15, &out[6]);
        return 0;
    }

    int tracker_encode_location2(const antz_tracker_location2_t* in,
                                 uint8_t* out, const uint8_t len) {
        if (!in || !out || len < PAGE_LEN) return -1;
        if (!index_fits(in->asset_index)) return -1;

        out[0] = ANTZ_TRACKER_PAGE_LOCATION_2;
        out[1] = index_byte(in->asset_index);
        uint16_to_bytes(in->latitude_bits_16_31, &out[2]);
        // Cast through unsigned, matching the decoder: the profile's
        // coordinates are signed two's complement and the bytes are the same
        // either way, but going via uint32_t is defined for every input.
        uint32_to_bytes(static_cast<uint32_t>(in->longitude_semicircles), &out[4]);
        return 0;
    }

    int tracker_encode_identification1(const antz_tracker_identification1_t* in,
                                       uint8_t* out, const uint8_t len) {
        if (!in || !out || len < PAGE_LEN) return -1;
        if (!index_fits(in->asset_index)) return -1;

        out[0] = ANTZ_TRACKER_PAGE_IDENTIFICATION_1;
        out[1] = index_byte(in->asset_index);
        out[2] = in->colour;
        return write_name(&out[3], in->name, 5);
    }

    int tracker_encode_identification2(const antz_tracker_identification2_t* in,
                                       uint8_t* out, const uint8_t len) {
        if (!in || !out || len < PAGE_LEN) return -1;
        if (!index_fits(in->asset_index)) return -1;

        out[0] = ANTZ_TRACKER_PAGE_IDENTIFICATION_2;
        out[1] = index_byte(in->asset_index);
        out[2] = in->asset_type;
        return write_name(&out[3], in->name, 5);
    }

    int tracker_encode_no_assets(uint8_t* out, const uint8_t len) {
        if (!out || len < PAGE_LEN) return -1;
        out[0] = ANTZ_TRACKER_PAGE_NO_ASSETS;
        memset(&out[1], RESERVED_BYTE, PAGE_LEN - 1);
        return 0;
    }

    int tracker_encode_disconnect(uint8_t* out, const uint8_t len) {
        if (!out || len < PAGE_LEN) return -1;
        out[0] = ANTZ_TRACKER_PAGE_DISCONNECT;
        memset(&out[1], RESERVED_BYTE, PAGE_LEN - 1);
        return 0;
    }

    void tracker_latitude_split(const int32_t latitude, uint16_t* bits_0_15,
                                uint16_t* bits_16_31) {
        const uint32_t raw = static_cast<uint32_t>(latitude);
        if (bits_0_15)  *bits_0_15  = static_cast<uint16_t>(raw & 0xFFFF);
        if (bits_16_31) *bits_16_31 = static_cast<uint16_t>(raw >> 16 & 0xFFFF);
    }

} // namespace antz
