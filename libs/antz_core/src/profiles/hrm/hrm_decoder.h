//
// Created by Kenneth Gulbrandsøy on 11/05/2025.
//

#pragma once

#include <stdint.h>

#include "hrm_pages.h"

// Decodes a raw ANT+ HRM packet into an antz_hrm_page_t structure
// raw: pointer to raw data starting at the HRM profile page
// len: length of the raw data (should be at least the size needed for decoding)
// out_page: pointer to struct to fill with decoded data
// Returns 0 on success, nonzero on decoded error
int hrm_decode_page0(const uint8_t* raw, uint8_t len, antz_hrm_page0_t* out_page);
