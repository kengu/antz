//
// Pure decoding for the ANT+ profiles this app speaks.
//
// Header-only and free of the ANT SDK on purpose: everything here is
// bit-twiddling over bytes already received, so it can be exercised without a
// dongle, a channel or a link. discovery.cpp calls it; tests/decode_test.cpp
// is the reason it is a separate header.
//
// References, cited per function:
//   TRK — ANT+ Device Profile, Tracker, Rev 1.0 (D00001671)
//   HRM — ANT+ Device Profile, Heart Rate, Rev 2.5
//

#ifndef ANT_DECODE_H
#define ANT_DECODE_H

#include <cstdint>
#include <optional>

namespace ant::decode {

// ─── ANT+ Tracker ──────────────────────────────────────────────────────────

// TRK Table 7-5. Values apply to a Dog asset; an Asset Tracker asset reports
// Undefined. Only 0-4 are defined; 5-7 fall in the 3-bit field but name nothing.
enum class AssetSituation : uint8_t {
    Sitting   = 0,
    Moving    = 1,
    Pointed   = 2,
    Treed     = 3,
    Unknown   = 4,
    Undefined = 255,
};

// TRK Table 7-4: situation occupies bits 0:2 of the Data Page 1 status byte.
// A whole-byte 0xFF is the Asset Tracker case — Table 7-5 gives that asset type
// Undefined, and 0xFF cannot fit the 3-bit field it would otherwise name.
inline AssetSituation situation(const uint8_t statusByte) {
    if (statusByte == 0xFF) return AssetSituation::Undefined;
    const uint8_t v = statusByte & 0x07;
    return v > static_cast<uint8_t>(AssetSituation::Unknown)
        ? AssetSituation::Undefined
        : static_cast<AssetSituation>(v);
}

// TRK Table 7-4: low battery 3, GPS lost 4, communication lost 5, remove 6.
inline bool lowBattery(const uint8_t s) { return (s & 0x08) != 0; }
inline bool gpsLost(const uint8_t s)    { return (s & 0x10) != 0; }
inline bool commsLost(const uint8_t s)  { return (s & 0x20) != 0; }
inline bool removeFlag(const uint8_t s) { return (s & 0x40) != 0; }

// TRK Table 7-3: asset index is bits 0:4 of byte 1, so at most 32 assets.
inline uint8_t assetIndex(const uint8_t b) { return b & 0x1F; }

// TRK §8.1: degrees = semicircles / 2^31 * 180, signed two's complement.
inline double semicirclesToDegrees(const int32_t semicircles) {
    return static_cast<double>(semicircles) * (180.0 / 2147483648.0);
}

// TRK §8.2: degrees = bradians / 256 * 360.
inline double bradiansToDegrees(const uint8_t bradians) {
    return static_cast<double>(bradians) * (360.0 / 256.0);
}

// ─── ANT+ Heart Rate ───────────────────────────────────────────────────────

// HRM Table 7: byte 0 is the page number in bits 0:6 with a toggle in bit 7.
inline uint8_t hrmPage(const uint8_t b)   { return b & 0x7F; }
inline bool    hrmToggle(const uint8_t b) { return (b & 0x80) != 0; }

// HRM Table 7 byte 7: Computed Heart Rate, 1-255 bpm, "If Invalid set to 0x00".
// Absent rather than zero, so a monitor that has not acquired cannot be read
// as a measurement of nought.
inline std::optional<uint8_t> computedHeartRate(const uint8_t b) {
    if (b == 0) return std::nullopt;
    return b;
}

// HRM Table 7 bytes 4-5: time of the last valid beat, 1/1024 s, rolls at 64 s.
inline uint16_t heartBeatEventTime(const uint8_t lsb, const uint8_t msb) {
    return static_cast<uint16_t>(static_cast<uint16_t>(msb) << 8 | lsb);
}

} // namespace ant::decode

#endif // ANT_DECODE_H
