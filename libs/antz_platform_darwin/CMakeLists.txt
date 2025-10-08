# antz_platform_darwin/CMakeLists.txt

option(ANTZ_PLATFORM "Platform for ANT+ SDK" "darwin")
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
else ()
    message(FATAL_ERROR "Unsupported platform: ${ANTZ_PLATFORM}")
endif()

if(ANT_SDK_LIB_DIR)
    link_directories(${ANT_SDK_LIB_DIR})
endif()

add_library(antz_platform_darwin STATIC src/darwin.cpp)

target_include_directories(antz_platform_darwin PRIVATE
    ${ANT_SDK_INCLUDE_DIRS}
)

target_link_libraries(antz_platform_darwin PRIVATE
    antz_core
    antz_platform
    antz_logging
    ${ANT_SDK_LIB}
)
