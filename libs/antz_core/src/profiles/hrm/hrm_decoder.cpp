#include "hrm_decoder.h"

#include <cstdint>

#include "data/antz_bytes.h"

namespace antz {

    int hrm_decode_page0(const uint8_t* raw, const uint8_t len, antz_hrm_page0_t* out_page) {
        if (!raw || !out_page || len < 5) return -1;

        out_page->heart_rate = raw[0];
        out_page->reserved   = raw[1];
        out_page->beat_time  = bytes_to_uint16(&raw[2]);
        out_page->beat_count = raw[4];

        return 0;
    }

}
