//
// Created by Kenneth Gulbrandsøy on 25/05/2025.
//

#pragma once

#include <optional>
#include <antz_channel.h>

typedef struct antz_context antz_context_t;

// Initialization parameters (can extend as needed)
struct antz_context_init_t {
    uint16_t usb_baud_rate = 50000;
    uint32_t usb_device_number = 0;
};

namespace antz {
    /// Initialize hardware resources.
    /// Returns a context handle on success, std:nullopt on failure
    std::optional<antz_context_t> init(const antz_context_init_t* params);

    /// Opens an ANT channel with a given channel config.
    /// Returns channel handle on success, std:nullopt on failure
    std::optional<antz_channel_t> open(antz_context_t* ctx, const antz_channel_config_t* cfg);

    /// Closes the previously opened ANT channel (safe to call from any channel state)
    void close(antz_context_t* ctx, const antz_channel_t* channel);

    /// Starts event loop for receiving/dispatching events.
    /// Returns true on success, false if already running or can't be started
    bool start(const antz_context_t* ctx);

    /// Stops the event loop cleanly; does not de-init the hardware (can re-start)
    void stop(const antz_context_t* ctx);

    /// Releases all hardware resources, closes all channels, and destroys context handle
    void shutdown(antz_context_t* ctx);

}
