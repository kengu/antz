#pragma once

#include <string>

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

}