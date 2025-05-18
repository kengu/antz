# Top-level CMakeLists.txt for antz
cmake_minimum_required(VERSION 3.16)
project(antz LANGUAGES C CXX)

# Enable verbose output
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Platform selection (default to darwin)
set(ANTZ_PLATFORM darwin CACHE STRING "Target platform (nrf5, darwin, linux, win)")
set_property(CACHE ANTZ_PLATFORM PROPERTY STRINGS nrf5 mac linux win)

# Output selected antz_platform
message(STATUS "Selected ANTZ platform: ${ANTZ_PLATFORM}")

# Core profile SDK (profile parsers, events, simulators)
add_subdirectory(libs/antz_core)

# Bridge module (translates profile data to BLE/cloud/etc.)
add_subdirectory(libs/antz_bridge)

# Platform-specific ANT transport + BLE/GATT bindings
if(ANTZ_PLATFORM STREQUAL "nrf5")
    add_subdirectory(libs/antz_platform/nrf5)
elseif(ANTZ_PLATFORM STREQUAL "darwin")
    message(STATUS "PLATFORM: ${ANTZ_PLATFORM}, evaluated type: ${ANTZ_PLATFORM} (length: ${ANTZ_PLATFORM_LENGTH})")
    add_subdirectory(libs/antz_platform/mac)
    message(STATUS "PLATFORM: ${ANTZ_PLATFORM}, evaluated type: ${ANTZ_PLATFORM} (length: ${ANTZ_PLATFORM_LENGTH})")
elseif(ANTZ_PLATFORM STREQUAL "linux")
    add_subdirectory(libs/antz_platform/linux)
elseif(ANTZ_PLATFORM STREQUAL "win")
    add_subdirectory(libs/antz_platform/win)
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
option(ANTZ_DISCOVERY "Include ant_discovery app" OFF)
if(ANTZ_DISCOVERY)
    add_subdirectory(apps/ant_discovery)
endif()