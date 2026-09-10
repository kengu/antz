//
// Pure decoding for the ANT+ profiles this app speaks.
//
// **The Tracker half is now a thin shim over libs/antz_core.** Per
// docs/architecture.md the parsers belong in antz_core/profiles/ and this app
// is the legacy monolith they were refactored out of. The names stay so that
// discovery.cpp and tests/decode_test.cpp are unchanged — which is the point:
// the tests that found these bugs against real hardware now run against the
// refactored module, and a behaviour change shows up as a test failure rather
// than as a second implementation quietly drifting.
//
// The HRM half has not moved yet. antz_core has hrm_decoder for page 0 and
// nothing for page 4, so folding this one into it would lose coverage.
//
// Header-only and free of the ANT SDK on purpose: everything here is
// bit-twiddling over bytes already received, so it can be exercised without a
// dongle, a channel or a link.
//
// References, cited per function:
//   TRK — ANT+ Device Profile, Tracker, Rev 1.0 (D00001671)
//   HRM — ANT+ Device Profile, Heart Rate, Rev 2.5
//

#ifndef ANT_DECODE_H
#define ANT_DECODE_H

#include <cstdint>
#include <optional>

#include "profiles/tracker/tracker_decoder.h"
#include "profiles/tracker/tracker_pages.h"

namespace ant::decode {

// ─── ANT+ Tracker ──────────────────────────────────────────────────────────

// TRK Table 7-5, as antz_core defines it. The enumerators keep their values,
// so a caller comparing against the numeric wire value is unaffected.
enum class AssetSituation : uint8_t {
    Sitting   = ANTZ_TRACKER_SITUATION_SITTING,
    Moving    = ANTZ_TRACKER_SITUATION_MOVING,
    Pointed   = ANTZ_TRACKER_SITUATION_POINTED,
    Treed     = ANTZ_TRACKER_SITUATION_TREED,
    Unknown   = ANTZ_TRACKER_SITUATION_UNKNOWN,
    Undefined = ANTZ_TRACKER_SITUATION_UNDEFINED,
};

inline AssetSituation situation(const uint8_t statusByte) {
    return static_cast<AssetSituation>(antz::tracker_decode_status(statusByte).situation);
}

inline bool lowBattery(const uint8_t s) { return antz::tracker_decode_status(s).low_battery; }
inline bool gpsLost(const uint8_t s)    { return antz::tracker_decode_status(s).gps_lost; }
inline bool commsLost(const uint8_t s)  { return antz::tracker_decode_status(s).comms_lost; }
inline bool removeFlag(const uint8_t s) { return antz::tracker_decode_status(s).remove; }

// TRK Table 7-3: asset index is bits 0:4 of byte 1, so at most 32 assets.
inline uint8_t assetIndex(const uint8_t b) { return b & 0x1F; }

inline double semicirclesToDegrees(const int32_t semicircles) {
    return antz::tracker_semicircles_to_degrees(semicircles);
}

inline double bradiansToDegrees(const uint8_t bradians) {
    return antz::tracker_bradians_to_degrees(bradians);
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
