# ant_discovery README

ant_discovery is a legacy, monolithic implementation of ANT+ discovery and profile 
decoding, originally created as a single-file prototype to explore how Garmin Asset Tracker 
and HRM profiles behave in real-time across multiple ANT channels.

This module is temporarily preserved during the migration to the modular 
`libs/antz_core`, `libs/antz_bridge`, and `libs/antz_platform` structure of the main `antz` project.

---

## Purpose
- Provides a self-contained implementation of ANT+ message parsing, device management, and profile-specific handling (Asset Tracker, HRM)
- Useful for regression testing, experimentation, and migration reference
- Bridges ANT+ messages from connected ANT USB dongles to terminal output, future BLE, or GUI consumers

---

## Structure
- Includes `discovery.cpp`: core event loop, ANT channel logic, and message dispatch
- Handles channel assignment, extended message decoding, and page-based data interpretation
- Contains initial implementation of event loop, retry logic, and basic device caching

---

## ⚙️ Build Instructions
This module is included conditionally from the top-level `CMakeLists.txt`:

```cmake
option(ANTZ_DISCOVERY "Include legacy ant_discovery module" ON)
if(ANTZ_DISCOVERY)
    add_subdirectory(ant_discovery)
endif()
```

To build:
```sh
cmake -B build/darwin -S . -DANTZ_PLATFORM=darwin -DANTZ_DISCOVERY=ON
cmake --build build/darwin --target ant_discovery
```

---

## Migration Plan
- Functionality in `discovery.cpp` is being split into modular components:
    - Profile decoding → `libs/core`
    - Data bridging → `libs/bridge`
    - Platform adapters → `libs/platform/<platform>`
- The logic will be reused in structured test apps and firmware (see `apps/`)

This module will be deprecated and removed once all features are ported into modular components.

---

## License
This file inherits the license from the `antz` project. See root-level `LICENSE` file for details.