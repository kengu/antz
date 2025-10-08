#pragma once

#include <stdint.h>

typedef enum {
    ANTZ_HRM_PAGE_0 = 0x00, // Default or Unknown Data
    ANTZ_HRM_PAGE_4 = 0x04, // Previous Heart Beat
    // Add other HRM page numbers as needed
} antz_hrm_page_e;

typedef struct {
    uint8_t heart_rate;
    uint8_t reserved;
    uint16_t beat_time;
    uint8_t beat_count;
} antz_hrm_page0_t;

// Add more HRM page structs here as needed