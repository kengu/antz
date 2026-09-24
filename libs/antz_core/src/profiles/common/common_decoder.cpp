#include "common_decoder.h"

#include "data/antz_bytes.h"

namespace antz {

    namespace {
        // A field a device declines to supply is all-ones, per D00001198.
        constexpr uint8_t  INVALID_U8  = 0xFF;
        constexpr uint32_t INVALID_U32 = 0xFFFFFFFFu;
        constexpr uint8_t  INVALID_COARSE_VOLTS = 0x0F;
    }

    int common_decode_manufacturer(const uint8_t* raw, const uint8_t len,
                                   antz_common_manufacturer_t* out) {
        if (!raw || !out || len < 8) return -1;
        if (raw[0] != ANTZ_COMMON_PAGE_MANUFACTURER) return -1;

        // Bytes 1-2 are reserved and transmitted as 0xFF. Not read: a device
        // that puts something there is not describing itself, and a decoder
        // that surfaced it would invite a consumer to depend on it.
        out->hw_revision     = raw[3];
        out->manufacturer_id = bytes_to_uint16(&raw[4]);
        out->model_number    = bytes_to_uint16(&raw[6]);
        return 0;
    }

    int common_decode_product(const uint8_t* raw, const uint8_t len,
                              antz_common_product_t* out) {
        if (!raw || !out || len < 8) return -1;
        if (raw[0] != ANTZ_COMMON_PAGE_PRODUCT) return -1;

        out->sw_revision_supplemental = raw[2];
        out->sw_revision_supplemental_valid = raw[2] != INVALID_U8;
        out->sw_revision_main = raw[3];

        const uint32_t serial = bytes_to_uint32(&raw[4]);
        out->serial = serial;
        // Zero is a real serial and all-ones is the refusal. Testing the
        // value instead of the flag would collapse every device that
        // declines into serial 4294967295, which is worse than none at all:
        // they would look like one device.
        out->serial_valid = serial != INVALID_U32;
        return 0;
    }

    int common_decode_request_data_page(const uint8_t* raw, const uint8_t len,
                                        antz_common_request_t* out) {
        if (!raw || !out || len < 8) return -1;
        if (raw[0] != ANTZ_COMMON_PAGE_REQUEST_DATA) return -1;
        out->slave_serial   = bytes_to_uint16(&raw[1]);
        out->descriptor_1   = raw[3];
        out->descriptor_2   = raw[4];
        out->transmit_count = raw[5] & 0x7F;
        out->acknowledged   = (raw[5] & 0x80) != 0;
        out->requested_page = raw[6];
        out->command_type   = raw[7];
        return 0;
    }

    int common_decode_battery(const uint8_t* raw, const uint8_t len,
                              antz_common_battery_t* out) {
        if (!raw || !out || len < 8) return -1;
        if (raw[0] != ANTZ_COMMON_PAGE_BATTERY) return -1;

        // Byte 1 is reserved.
        out->identifier_valid = raw[2] != INVALID_U8;
        out->battery_count = out->identifier_valid ? (raw[2] & 0x0F) : 0;
        out->battery_identifier = out->identifier_valid ? (raw[2] >> 4) : 0;

        out->operating_time_ticks = (uint32_t)raw[3] | ((uint32_t)raw[4] << 8) | ((uint32_t)raw[5] << 16);
        out->operating_time_resolution_s = (raw[7] & 0x80) ? 2u : 16u;
        out->operating_time_s = out->operating_time_ticks * out->operating_time_resolution_s;

        // Coarse volts in byte 7's low nibble, fractional in byte 6 as
        // 1/256 V. The fraction means nothing without the whole volts.
        out->voltage_coarse = raw[7] & 0x0F;
        out->voltage_fractional = raw[6];
        out->voltage_valid = out->voltage_coarse != INVALID_COARSE_VOLTS;
        out->voltage_v = out->voltage_valid
            ? (float)out->voltage_coarse + (float)out->voltage_fractional / 256.0f
            : 0.0f;

        const uint8_t status = (raw[7] >> 4) & 0x07;
        out->status = status;
        out->status_valid = status >= ANTZ_BATTERY_STATUS_NEW && status <= ANTZ_BATTERY_STATUS_CRITICAL;
        return 0;
    }

} // namespace antz
