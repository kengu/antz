#include "tracker_decoder.h"

#include <cstring>

#include "data/antz_bytes.h"

namespace antz {

    namespace {
        // TRK Table 7-3: the index is bits 0:4 of byte 1, with the top three
        // bits reserved and set to 0x7. Forgetting the mask reads index 3 as
        // 227.
        constexpr uint8_t ASSET_INDEX_MASK = 0x1F;

        // TRK Table 7-4.
        constexpr uint8_t SITUATION_MASK   = 0x07;
        constexpr uint8_t FLAG_LOW_BATTERY = 0x08;
        constexpr uint8_t FLAG_GPS_LOST    = 0x10;
        constexpr uint8_t FLAG_COMMS_LOST  = 0x20;
        constexpr uint8_t FLAG_REMOVE      = 0x40;

        // TRK Table 7-5: an Asset Tracker asset has no situation.
        constexpr uint8_t STATUS_UNDEFINED = 0xFF;

        // Names are 0x00-padded on the wire rather than terminated.
        void copy_name(char* dst, const uint8_t* src, size_t n) {
            size_t i = 0;
            for (; i < n && src[i] != 0x00; ++i) dst[i] = static_cast<char>(src[i]);
            dst[i] = '\0';
        }
    }

    antz_tracker_status_t tracker_decode_status(const uint8_t status_byte) {
        antz_tracker_status_t out{};
        out.raw = status_byte;

        if (status_byte == STATUS_UNDEFINED) {
            // Not a set of flags. Reading bits out of an all-ones byte
            // reports a low battery, a lost GPS, lost comms and a removal
            // that are all fictional.
            out.situation_valid = false;
            out.situation = ANTZ_TRACKER_SITUATION_UNDEFINED;
            return out;
        }

        out.situation_valid = true;
        // 5-7 fit the 3-bit field and name nothing, so they are UNDEFINED
        // rather than cast to an enumerator that does not exist. The flags
        // beside them are still readable — only the situation is unnamed.
        const uint8_t value = status_byte & SITUATION_MASK;
        out.situation = value > ANTZ_TRACKER_SITUATION_UNKNOWN
            ? ANTZ_TRACKER_SITUATION_UNDEFINED
            : static_cast<antz_tracker_situation_e>(value);
        out.low_battery = (status_byte & FLAG_LOW_BATTERY) != 0;
        out.gps_lost    = (status_byte & FLAG_GPS_LOST) != 0;
        out.comms_lost  = (status_byte & FLAG_COMMS_LOST) != 0;
        out.remove      = (status_byte & FLAG_REMOVE) != 0;
        return out;
    }

    int tracker_decode_location1(const uint8_t* raw, const uint8_t len,
                                 antz_tracker_location1_t* out) {
        if (!raw || !out || len < 8) return -1;

        out->asset_index        = raw[1] & ASSET_INDEX_MASK;
        out->distance_m         = bytes_to_uint16(&raw[2]);
        out->bearing_bradians   = raw[4];
        out->status             = tracker_decode_status(raw[5]);
        out->latitude_bits_0_15 = bytes_to_uint16(&raw[6]);
        return 0;
    }

    int tracker_decode_location2(const uint8_t* raw, const uint8_t len,
                                 antz_tracker_location2_t* out) {
        if (!raw || !out || len < 8) return -1;

        out->asset_index         = raw[1] & ASSET_INDEX_MASK;
        out->latitude_bits_16_31 = bytes_to_uint16(&raw[2]);
        // Read unsigned and cast: the profile's coordinates are signed two's
        // complement, and treating them as unsigned puts a southern or
        // western asset in the wrong hemisphere.
        out->longitude_semicircles =
            static_cast<int32_t>(bytes_to_uint32(&raw[4]));
        return 0;
    }

    int tracker_decode_identification1(const uint8_t* raw, const uint8_t len,
                                       antz_tracker_identification1_t* out) {
        if (!raw || !out || len < 8) return -1;

        out->asset_index = raw[1] & ASSET_INDEX_MASK;
        out->colour      = raw[2];
        copy_name(out->name, &raw[3], 5);
        return 0;
    }

    int tracker_decode_identification2(const uint8_t* raw, const uint8_t len,
                                       antz_tracker_identification2_t* out) {
        if (!raw || !out || len < 8) return -1;

        out->asset_index = raw[1] & ASSET_INDEX_MASK;
        // No default. Zero is a real Asset Type — "Asset Tracker", a person —
        // so a caller must distinguish "not yet seen" from "seen as 0" by
        // whether this page arrived at all.
        out->asset_type  = raw[2];
        copy_name(out->name, &raw[3], 5);
        return 0;
    }

    int32_t tracker_latitude(const uint16_t bits_0_15, const uint16_t bits_16_31) {
        const uint32_t raw = (static_cast<uint32_t>(bits_16_31) << 16) |
                             static_cast<uint32_t>(bits_0_15);
        return static_cast<int32_t>(raw);
    }

    double tracker_semicircles_to_degrees(const int32_t semicircles) {
        return static_cast<double>(semicircles) * (180.0 / 2147483648.0);
    }

    double tracker_bradians_to_degrees(const uint8_t bradians) {
        return static_cast<double>(bradians) * (360.0 / 256.0);
    }

} // namespace antz
