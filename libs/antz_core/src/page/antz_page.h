#pragma once

#include <data/antz_data.h>
#include <variant>
#include <array>
#include <cstdint>
#include <optional>

#include "profiles/antz_profile.h"
#include "profiles/hrm/hrm_pages.h"

// Event data variant to hold different decoded ANT+ page/profile structs
typedef std::variant<
    antz_hrm_page0_t
> ant_page_data_t;

struct ant_page_t {
    uint8_t number;
    ant_profile_e profile;
    ant_page_data_t data;
};

namespace antz {

    /**
     * Processes ant_data_t and extracts an ant_page_t if matched.
     *
     * @param data A pointer to the ANT parsed from the ANT message.
     * @return An optional ant_page_t object if the msg_id matches the
     *          expected values, otherwise returns std::nullopt.
     */
    std::optional<ant_page_t> handle_ant_page(const ant_data_t* data);

} // namespace antz