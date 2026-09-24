#include "common_encoder.h"

namespace antz {

    int common_encode_request_data_page(const antz_common_request_t* in,
                                        uint8_t* out, const uint8_t len) {
        if (!in || !out || len < 8) return -1;
        if (in->transmit_count == 0 || in->transmit_count > 0x7F) return -1;
        if (in->command_type < ANTZ_REQUEST_DATA_PAGE ||
            in->command_type > ANTZ_REQUEST_DATA_PAGE_SET) return -1;

        out[0] = ANTZ_COMMON_PAGE_REQUEST_DATA;
        out[1] = (uint8_t)(in->slave_serial & 0xFF);
        out[2] = (uint8_t)(in->slave_serial >> 8);
        out[3] = in->descriptor_1;
        out[4] = in->descriptor_2;
        out[5] = (uint8_t)(in->transmit_count | (in->acknowledged ? 0x80 : 0x00));
        out[6] = in->requested_page;
        out[7] = in->command_type;
        return 0;
    }

} // namespace antz
