# Top-level CMakeLists.txt for antz
cmake_minimum_required(VERSION 3.16)
project(antz LANGUAGES C CXX)

# Enable minimum cxx version (see docs/cxx-requirements.md)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable verbose output
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Platform selection (default to darwin)
set(ANTZ_PLATFORM darwin CACHE STRING "Target platform (nrf5, darwin, linux, win)")
set_property(CACHE ANTZ_PLATFORM PROPERTY STRINGS nrf5 darwin linux win)

# Output selected antz_platform
message(STATUS "Selected ANTZ platform: ${ANTZ_PLATFORM}")

# Core module (antz data, pages, event)
add_subdirectory(libs/antz_core)

# Platform module (hal implementation helpers)
add_subdirectory(libs/antz_platform)

# Bridge module (translates profile data to BLE/cloud/etc.)
add_subdirectory(libs/antz_bridge)

# Platform-specific ANT transport + BLE/GATT bindings
if(ANTZ_PLATFORM STREQUAL "nrf5")
    add_subdirectory(libs/antz_platform_nrf5)
elseif(ANTZ_PLATFORM STREQUAL "darwin")
    add_subdirectory(libs/antz_platform_darwin)
elseif(ANTZ_PLATFORM STREQUAL "linux")
    add_subdirectory(libs/antz_platform_linux)
elseif(ANTZ_PLATFORM STREQUAL "win")
    add_subdirectory(libs/antz_platform_win)
else()
    message(FATAL_ERROR "Unsupported platform: ${ANTZ_PLATFORM}")
endif()

# Platform-specific SDK build targets
if(ANTZ_PLATFORM STREQUAL "darwin")
    add_subdirectory(sdks/ANT-SDK_Mac.3.5)
    add_custom_target(full_sdk_build DEPENDS full_build)
else()
    message(WARNING "ANT SDK for Platform ${ANTZ_PLATFORM} have no full_sdk_build target")
endif()

# Optional: Include legacy monolithic ANT+ discovery module during migration
option(ANTZ_DISCOVERY "Include ant_discovery app" ON)
if(ANTZ_DISCOVERY)
    add_subdirectory(apps/ant_discovery)
endif()