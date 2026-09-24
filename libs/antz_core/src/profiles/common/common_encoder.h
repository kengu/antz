//
// ANT+ Common Data Page encoding. See common_pages.h.
//

#pragma once

#include <stdint.h>

#include "common_pages.h"

namespace antz {

    // Page 70, every field as given. Writes eight bytes and returns 0;
    // nonzero, leaving `out` untouched, when `in` or `out` is null, `len` is
    // short, the transmit count is 0 or above 127 (byte 5's zero is the
    // invalid value, and bit 7 is the acknowledged flag), or the command type
    // is not 1-4.
    //
    // A profile decides which command type a request uses; this does not.
    // For the Tracker profile's rule, use tracker_encode_request.
    int common_encode_request_data_page(const antz_common_request_t* in,
                                        uint8_t* out, uint8_t len);

} // namespace antz
