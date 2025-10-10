cmake_minimum_required(VERSION 3.16)

project(ant_discovery)

option(ANTZ_PLATFORM "Platform for ANT+ SDK" "linux")
set(ANT_SDK_BASE "${CMAKE_SOURCE_DIR}/sdks")

# Platform-specific SDK config (expandable for multi-platform)
if(ANTZ_PLATFORM STREQUAL "darwin")
    set(ANT_SDK_PATH "${ANT_SDK_BASE}/ANT-SDK_Mac.3.5")
    set(ANT_SDK_LIB antbase)
    set(ANT_SDK_INCLUDE_DIRS
        "${ANT_SDK_PATH}/bin/include"
        "${ANT_SDK_PATH}/bin/include/inc"
        "${ANT_SDK_PATH}/bin/include/common"
        "${ANT_SDK_PATH}/bin/include/software"
        "${ANT_SDK_PATH}/bin/include/software/ANTFS"
        "${ANT_SDK_PATH}/bin/include/software/serial"
        "${ANT_SDK_PATH}/bin/include/software/serial/device_management"
        "${ANT_SDK_PATH}/bin/include/software/system"
        "${ANT_SDK_PATH}/bin/include/software/USB"
        "${ANT_SDK_PATH}/bin/include/software/USB/devices"
        "${ANT_SDK_PATH}/bin/include/software/USB/device_handles"
        "${ANT_SDK_PATH}/bin/include/software/USB/iokit_driver"
    )
    set(ANT_SDK_LIB_DIR "${ANT_SDK_PATH}/bin")
elseif(ANTZ_PLATFORM STREQUAL "linux")
    set(ANT_SDK_PATH "${ANT_SDK_BASE}/ANT-SDK_Linux_3.8.200_BETA")
    set(ANT_SDK_LIB antbase)
    set(ANT_SDK_INCLUDE_DIRS
        "${ANT_SDK_PATH}/ANT_DLL"
        "${ANT_SDK_PATH}/ANT_LIB"
        "${ANT_SDK_PATH}/ANT_LIB/inc"
        "${ANT_SDK_PATH}/ANT_LIB/common"
        "${ANT_SDK_PATH}/ANT_LIB/software"
        "${ANT_SDK_PATH}/ANT_LIB/software/ANTFS"
        "${ANT_SDK_PATH}/ANT_LIB/software/serial"
        "${ANT_SDK_PATH}/ANT_LIB/software/serial/device_management"
        "${ANT_SDK_PATH}/ANT_LIB/software/system"
        "${ANT_SDK_PATH}/ANT_LIB/software/USB"
        "${ANT_SDK_PATH}/ANT_LIB/software/USB/devices"
        "${ANT_SDK_PATH}/ANT_LIB/software/USB/device_handles"
        "${ANT_SDK_PATH}/ANT_LIB/software/USB/iokit_driver"
        "${ANT_SDK_PATH}/ANT_LIB/software/serial"
    )
    set(ANT_SDK_LIB_DIR "${ANT_SDK_PATH}/bin")
else ()
    message(FATAL_ERROR "Unsupported platform: ${ANTZ_PLATFORM}")
endif()

set(SRC_FILES
    src/main.cpp
    src/usb_checker.cpp
    src/discovery.cpp
    src/hrm_discovery.cpp
    src/asset_tracker_discovery.cpp
    src/logging.h
    src/ant_device.h
    src/ant_profiles.h
    src/ant_constants.h
)

add_executable(ant_discovery ${SRC_FILES})

target_include_directories(ant_discovery PRIVATE
    src
    ${ANT_SDK_INCLUDE_DIRS}
)

target_compile_definitions(ant_discovery PRIVATE DEBUG_FILE)

if(ANT_SDK_LIB_DIR)
    link_directories(${ANT_SDK_LIB_DIR})
endif()

target_link_libraries(ant_discovery PRIVATE ${ANT_SDK_LIB})

# Output to monorepo-idiomatic location (e.g., apps/ant_discovery/bin)
set_target_properties(ant_discovery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

message(STATUS "ant_discovery output path: ${CMAKE_CURRENT_SOURCE_DIR}/bin")