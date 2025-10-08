//
// Created by Kenneth Gulbrandsøy on 25/05/2025.
//

#pragma once

#include <antz_context.h>

/// Opaque pointer to antz hardware abstraction layer (hal).
/// Platforms implement antz_hal hiding (making it opaque)
/// internal details it needs to function, like pointers to
/// hardware resources, etc.
typedef struct antz_hal antz_hal_t;

/// Opaque pointer to ANT channel implementation handle.
typedef struct antz_hal_channel antz_hal_channel_t;

namespace antz_platform {

    /// Initializes hardware resources.
    /// Returns a hal handle on success, std:nullopt on failure
    std::optional<antz_hal_t*> antz_hal_create(const antz_context_init_t* params);

    /// Opens an ANT channel with a given channel config.
    /// Returns channel handle on success, std:nullopt on failure
    std::optional<antz_hal_channel_t*> antz_hal_open(antz_hal* hal, const antz_channel_config_t* cfg);

    /// Closes the previously opened ANT channel (safe to call from any channel state)
    void antz_hal_close(antz_hal* hal, antz_hal_channel_t* channel);

    /// Starts event loop for receiving/dispatching events.
    /// Returns true on success, false if already running or can't be started
    bool antz_hal_start(antz_hal* hal);

    /// Stops the event loop cleanly; does not de-init the hardware (can re-start)
    void antz_hal_stop(antz_hal* hal);

    /// Releases all hardware
    void antz_hal_destroy(antz_hal_t* hal);

}
