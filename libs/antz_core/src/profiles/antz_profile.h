//
// Created by Kenneth Gulbrandsøy on 18/05/2025.
//

#pragma once

#include <cstdint>

typedef enum {
    NONE = 0,
    HRM,
    TRACKER,
    // Add more profiles as needed
} ant_profile_e;

// Device Type constants as per ANT+ definitions
#define ANT_DEVICE_TYPE_HRM         0x78
#define ANT_DEVICE_TYPE_TRACKER     0x29

static ant_profile_e ant_profile_from_device_type(const uint8_t device_type)
{
    switch(device_type) {
    case ANT_DEVICE_TYPE_HRM: return HRM;
    case ANT_DEVICE_TYPE_TRACKER: return TRACKER;
    default: return NONE;
    }
}
