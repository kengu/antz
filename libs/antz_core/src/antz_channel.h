//
// Created by Kenneth Gulbrandsøy on 18/05/2025.
//

#pragma once

#include "profiles/antz_profile.h"

/// Opaque channel handle.
typedef struct antz_channel antz_channel_t;

typedef struct{
    uint8_t id;
    ant_profile_e profile;
    /* device number, device type, transmission type, etc.*/
} antz_channel_config_t;

