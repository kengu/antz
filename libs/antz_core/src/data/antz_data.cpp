//
// Created by Kenneth Gulbrandsøy on 18/05/2025.
//

#include <antz_core_logging.h>
#include <data/antz_data.h>
#include <data/antz_bytes.h>
#include <logger/antz_logger.h>
#include <cstdint>
#include <optional>

namespace antz
{
    // Rx Timestamp messaging is enabled if message data[12] is 0x20
    constexpr uint8_t RX_TIMESTAMP_FLAG     = 0x20;

    // RSSI extended messaging is enabled if message data[12] is 0x40
    constexpr uint8_t RSSI_EXT_FLAG         = 0x40;

    // Channel ID extended messaging is enabled if message data[12] is 0x80
    constexpr uint8_t CHANNEL_ID_EXT_FLAG   = 0x80;

    // Assumes data format
    // Rx Timestamp # (2) | Checksum (1)
    inline bool is_rx_timestamp_flag(const uint8_t flags) {
        return flags & RX_TIMESTAMP_FLAG;
    }

    // Assumes flag 0x20 @ data[9]
    inline bool is_rx_timestamp_ext(const uint8_t* d) {
        return is_rx_timestamp_flag(d[9]);
    }

    // Assumes data format
    // Channel # (1) | Payload (8) | Flag (1) | Device # (2) | Device Type (2) | Trans Type (1) | Checksum (1)
    inline bool is_device_channel_id_flag(const uint8_t flags) {
        return flags & CHANNEL_ID_EXT_FLAG;
    }

    // Assumes flag 0x80 @ data[9]
    inline bool is_device_channel_id_ext(const uint8_t* d) {
        return is_device_channel_id_flag(d[9]);
    }

    // Assumes data format
    // Channel # (1) | Payload (8) | Flag (1) | Measurement Type (2) | RSSI Value (1) | Threshold (1) | Checksum (1)
    inline bool is_rssi_flag(const uint8_t flags) {
        return flags & RSSI_EXT_FLAG;
    }

    // Assumes flag 0x40 @ data[9]
    inline bool is_rssi_ext(const uint8_t* d) {
        return d[9] & RSSI_EXT_FLAG;
    }

    inline u_int16_t parse_rx_ts(const uint8_t* trailer) {
        return bytes_to_uint16(trailer);
    }

    inline u_int16_t parse_device_number(const uint8_t* trailer) {
        return bytes_to_uint16(trailer);
    }

    inline const char* describe_device_type(const uint8_t type) {
        switch (type) {
        case 0x29: return "Asset Tracker";
        case 0x78: return "Heart Rate Monitor (HRM)";
        case 0x7B: return "Bike Speed Sensor";
        case 0x7C: return "Bike Speed/Cadence Sensor";
        case 0x0F: return "Generic GPS (Garmin)";
        case 0x30: return "Temperature Sensor";
        case 0x0D: return "Stride Sensor";
        case 0x79: return "Garmin Dog Collar (proprietary)";
        default:
            thread_local char buf[64];
            snprintf(buf, sizeof(buf), "Unknown (0x%s)", antz::to_hex_byte(type));
            return buf;
        }
    }

    inline const char* format_ext_flags(const uint8_t flags) {
        return antz::formatf("Flags: 0x%s | ", antz::to_hex_byte(flags));
    }

    inline const char* format_device_channel_id(const ant_ext_fields_t& ext) {
        const auto info= ext.device_channel_id;
        return antz::formatf(
            "Device Type: 0x%s '%s' | Device #: 0x%s | Tx Type: 0x%s",
            antz::to_hex_byte(info->device_type),
            describe_device_type(info->device_type),
            antz::to_hex_byte(info->device_number),
            antz::to_hex_byte(info->transmission_type)
        );
    }

    inline const char* format_rssi(const ant_ext_fields_t& ext) {
        const auto info= ext.rssi;
        return antz::formatf(
            "Rssi Type: 0x%s '%s' | Rssi #: %d | Rssi Threshold: %d",
            antz::to_hex_byte(info->type),
            info->value,
            info->threshold
        );
    }

    bool parse_ext_fields(const uint8_t* data, const uint8_t length, ant_ext_fields_t& ext) {

        if (length < 10) {
            ANTZ_CORE_LOG_NO_EXT_INFO();
            return false;
        }

        uint8_t offset = 0;
        ext.flags = data[9];

        if (is_device_channel_id_ext(data)) {
            const uint8_t* trailer = &data[10];
            const auto info = &ext.device_channel_id.value();

            info->device_number = parse_device_number(trailer);
            offset = 2;

            info->device_type = static_cast<uint8_t>(trailer[offset++]);
            info->transmission_type = static_cast<uint8_t>(trailer[offset++]);
        }

        if (is_rssi_ext(data)) {
            const uint8_t* trailer = &data[10];
            const auto info = &ext.rssi.value();

            info->type = static_cast<uint8_t>(trailer[offset++]);
            info->value = static_cast<int8_t>(trailer[offset++]); // signed!
            info->threshold = static_cast<uint8_t>(trailer[offset++]);
        }

        if (is_rx_timestamp_ext(data)) {
            const uint8_t* trailer = &data[10];
            const auto info = &ext.rx_timestamp.value();

            info->timestamp_ms = parse_rx_ts(trailer);
            offset += 2;
        }

        ext.length = offset;

        if (10 + offset > length) {
            ANTZ_CORE_LOG_EXT_INFO_LENGTH_EXCEEDED_MESSAGE_LENGTH();
            return false;
        }

        return ext.length > 0;
    }

    constexpr uint8_t MESG_BROADCAST_DATA_ID = 0x4E;
    constexpr uint8_t MESG_EXT_BROADCAST_DATA_ID = 0x5D;

    std::optional<ant_data_t> handle_ant_message(const uint8_t msg_id, const uint8_t* data, const size_t data_len) {

        if (msg_id != MESG_BROADCAST_DATA_ID && msg_id != MESG_EXT_BROADCAST_DATA_ID) {
            return std::nullopt;
        }

        ant_data_t data_out;
        data_out.msg_id = msg_id;
        data_out.msg_len = data_len;

        const auto exists = parse_ext_fields(data, data_len, *data_out.ext);

        ANTZ_CORE_LOG_BROADCAST_RAW(
            data[0],
            msg_id,
            [&]{
                if (exists) {
                    return antz::formatf("%s | %s",
                        format_ext_flags(*data_out.ext->flags),
                        format_device_channel_id(*data_out.ext)
                    );
                }
                const uint8_t flags = data[9];
                return format_ext_flags(flags);
            }(),
            data_len,
            antz::to_hex(data, data_len)
        );

        // TODO: Detect if device channel id is present
        for (size_t i = 0; i < data_len; ++i) {
            data_out.payload[i] = data[i];
        }
        return data_out;
    }


}
