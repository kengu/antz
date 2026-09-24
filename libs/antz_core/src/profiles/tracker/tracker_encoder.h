//
// ANT+ Asset Tracker page encoding — the inverse of tracker_decoder.h.
//
// Here and not in a consumer, for the reason the decoder gives: the bugs in
// this profile were found against real Garmin hardware, and a second copy is
// a second place for them to come back. An encoder written beside a consumer's
// own decoder would agree with that decoder and with nothing else, which is
// how the four Rev 1.0 errata survived long enough to reach production once.
//
// Two things this is and is not:
//
//   - It **is** the inverse of the decoder in this library, held to the same
//     vectors under packages/platform-messages/spec/wire/ant/vectors/ in the
//     streamdog-platform repository, and to a round trip over the whole
//     parameter space rather than over those fixed points alone. That round
//     trip is worth as much as the encoder: the decoder has only ever been
//     tested one way.
//   - It is **not** evidence that a real handheld would send these bytes. The
//     vectors are authored from the specification, not captured off hardware,
//     so what agreement with them proves is that this library reads and writes
//     the profile the same way — with the errata already corrected. Treat
//     encoded pages as a test and simulation surface, never as a recording.
//
// Deprecated by ANT+ and frozen, which is what makes encoding safe: the
// layouts cannot change under us.
//

#pragma once

#include <stdint.h>

#include "tracker_pages.h"

namespace antz {

    // Encode the Asset Location 1 status byte — the inverse of
    // tracker_decode_status.
    //
    // `situation_valid == false` yields the 0xFF sentinel and ignores every
    // flag, because that is what the sentinel means: an Asset Tracker asset
    // has no situation, and the flags are not readable beside it either.
    //
    // A situation of ANTZ_TRACKER_SITUATION_UNDEFINED with situation_valid
    // set is the one input with no faithful encoding — the decoder produces
    // it for the unnamed values 5 through 7, which name nothing to write
    // back. It encodes as 5, the lowest of them, and round-trips to
    // UNDEFINED again.
    uint8_t tracker_encode_status(const antz_tracker_status_t* status);

    // Each writes exactly eight bytes to `out`, including the page number at
    // byte 0, and returns 0 on success. Nonzero when `in` or `out` is null,
    // `len` is short of the page, or a field is outside what the profile can
    // carry — an asset_index above 0x1F, or a name that does not fit.
    //
    // Reserved bits are written as the profile specifies rather than zeroed:
    // the asset index sits in bits 0:4 of byte 1 with the top three set to
    // 0x7, and a page built with those clear is one the decoder accepts and
    // real hardware would not have sent.
    int tracker_encode_location1(const antz_tracker_location1_t* in,
                                 uint8_t* out, uint8_t len);
    int tracker_encode_location2(const antz_tracker_location2_t* in,
                                 uint8_t* out, uint8_t len);
    int tracker_encode_identification1(const antz_tracker_identification1_t* in,
                                       uint8_t* out, uint8_t len);
    int tracker_encode_identification2(const antz_tracker_identification2_t* in,
                                       uint8_t* out, uint8_t len);

    // Pages 3 and 32 carry no asset: the roster is empty, or the channel is
    // closing. Both are a page number and seven reserved bytes.
    int tracker_encode_no_assets(uint8_t* out, uint8_t len);
    int tracker_encode_disconnect(uint8_t* out, uint8_t len);

    // A display's Common Page 70 request to an asset tracker, by the Tracker
    // profile's rule (TRK §7.10.1): data page 16 is requested with command
    // type 4, and the tracker answers with pages 16 and 17 for every asset;
    // every other page with command type 1 [SD_0014]. Serial and descriptors
    // invalid, broadcast replies. `transmit_count` is how many times the page
    // should be sent, 1-127. Same refusals as common_encode_request_data_page.
    int tracker_encode_request(uint8_t requested_page, uint8_t transmit_count,
                               uint8_t* out, uint8_t len);

    // Split a latitude back into the halves the profile spreads across the
    // two Asset Location pages — the inverse of tracker_latitude.
    void tracker_latitude_split(int32_t latitude, uint16_t* bits_0_15,
                                uint16_t* bits_16_31);

} // namespace antz
