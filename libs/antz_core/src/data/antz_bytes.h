#pragma once

#include <string>
#include <cstdint>

namespace antz
{
    
    // Combine little-endian bytes into an uint16_t
    inline uint16_t bytes_to_uint16(const uint8_t* bytes) {
        return static_cast<uint16_t>(bytes[1]) << 8 | bytes[0];
    }

    // Combine little-endian bytes into an uint32_t
    inline uint32_t bytes_to_uint32(const uint8_t* bytes) {
        return static_cast<uint32_t>(bytes[3]) << 24 |
               static_cast<uint32_t>(bytes[2]) << 16 |
               static_cast<uint32_t>(bytes[1]) << 8  |
               bytes[0];
    }

    // Split an uint16_t into little-endian bytes — the inverse of
    // bytes_to_uint16.
    inline void uint16_to_bytes(const uint16_t value, uint8_t* bytes) {
        bytes[0] = static_cast<uint8_t>(value & 0xFF);
        bytes[1] = static_cast<uint8_t>(value >> 8 & 0xFF);
    }

    // Split an uint32_t into little-endian bytes — the inverse of
    // bytes_to_uint32.
    inline void uint32_to_bytes(const uint32_t value, uint8_t* bytes) {
        bytes[0] = static_cast<uint8_t>(value & 0xFF);
        bytes[1] = static_cast<uint8_t>(value >> 8 & 0xFF);
        bytes[2] = static_cast<uint8_t>(value >> 16 & 0xFF);
        bytes[3] = static_cast<uint8_t>(value >> 24 & 0xFF);
    }

}
