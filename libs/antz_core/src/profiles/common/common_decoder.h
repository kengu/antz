//
// ANT+ Common Data Page decoding. See common_pages.h.
//

#pragma once

#include <stdint.h>

#include "common_pages.h"

namespace antz {

    // Each returns 0 on success and nonzero when `raw` is null, `out` is
    // null, `len` is short of the page, or the page number is not this
    // page's. `raw` points at the page number, so a full eight-byte ANT+
    // broadcast payload is what to pass.
    //
    // The page-number check is here and not in the tracker decoders because
    // these two are reached by dispatch on a shared page space: a caller
    // routing 0x50 to the product decoder would otherwise read a
    // manufacturer id as a serial and be told nothing.
    int common_decode_manufacturer(const uint8_t* raw, uint8_t len,
                                   antz_common_manufacturer_t* out);

    int common_decode_product(const uint8_t* raw, uint8_t len,
                              antz_common_product_t* out);

    int common_decode_battery(const uint8_t* raw, uint8_t len,
                              antz_common_battery_t* out);

} // namespace antz
