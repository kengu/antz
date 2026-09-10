//
// ANT+ Asset Tracker page decoding.
//
// Refactored out of apps/ant_discovery, which is where this logic ran first
// and where its bugs were found against real Garmin hardware. Four of those
// are worth naming, because a reimplementation tends to reintroduce them:
//
//   - `situation` is bits 0:2 of the status byte, not 5:7. Reading the wrong
//     end decodes every dog as Sitting and invents faults from the situation
//     bits.
//   - Semicircles are *signed*. Read unsigned, a dog south of the equator or
//     west of Greenwich lands in the wrong hemisphere — invisible in Norway.
//   - Asset Type has no sensible default. Zero means "Asset Tracker", so a
//     struct that starts zeroed reports every unknown asset as a person.
//   - Status 0xFF is a sentinel, not a set of flags. An Asset Tracker asset
//     has no situation, and reading bits out of an all-ones byte reports
//     four faults that are not there.
//
// Held to packages/platform-messages/spec/wire/ant/vectors/ in the
// streamdog-platform repository, which is also what the Streamdog C SDK's
// ANT adapter is held to — the two must agree, and this is the copy that
// decides.
//

#pragma once

#include <stdint.h>

#include "tracker_pages.h"

namespace antz {

    // Decode the Asset Location 1 status byte. Never fails: every value is
    // meaningful, including 0xFF.
    antz_tracker_status_t tracker_decode_status(uint8_t status_byte);

    // Each returns 0 on success and nonzero when `raw` is null, `out` is
    // null, or `len` is short of the page. `raw` points at the page number,
    // so a full eight-byte ANT+ broadcast payload is what to pass.
    int tracker_decode_location1(const uint8_t* raw, uint8_t len,
                                 antz_tracker_location1_t* out);
    int tracker_decode_location2(const uint8_t* raw, uint8_t len,
                                 antz_tracker_location2_t* out);
    int tracker_decode_identification1(const uint8_t* raw, uint8_t len,
                                       antz_tracker_identification1_t* out);
    int tracker_decode_identification2(const uint8_t* raw, uint8_t len,
                                       antz_tracker_identification2_t* out);

    // Assemble a latitude from the two halves the profile splits it across.
    // Signed, because semicircles are.
    int32_t tracker_latitude(uint16_t bits_0_15, uint16_t bits_16_31);

    // deg = semicircles / 2^31 * 180
    double tracker_semicircles_to_degrees(int32_t semicircles);

    // deg = bradians / 256 * 360
    double tracker_bradians_to_degrees(uint8_t bradians);

} // namespace antz
