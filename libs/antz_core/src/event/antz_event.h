#pragma once

#include <page/antz_page.h>

typedef enum {
    ANTZ_EVT_NONE = 0,
    ANTZ_EVT_CHANNEL_OPENED,
    ANTZ_EVT_CHANNEL_CLOSED,
    ANTZ_EVT_PAGE_RECEIVED,
    ANTZ_EVT_SHUTDOWN,
} antz_event_type_t;

// Event data variant to hold different
// data structs depending on the event type
typedef std::variant<
    ant_page_t
> ant_event_data_t;


struct antz_event_t {
    antz_event_type_t type = ANTZ_EVT_NONE;
    uint8_t channel = 0;
    uint32_t timestamp_ms = 0;
    ant_profile_e profile = NONE;
    ant_event_data_t data{};
};

