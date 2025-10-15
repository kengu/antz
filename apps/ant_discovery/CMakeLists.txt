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

    # ---- Mosquitto (MQTT) discovery for macOS (Homebrew) ----
    set(MOSQ_FOUND FALSE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MOSQUITTO QUIET IMPORTED_TARGET mosquitto)
        if(TARGET PkgConfig::MOSQUITTO)
            set(MOSQ_FOUND TRUE)
            set(MOSQUITTO_LINK_TARGET PkgConfig::MOSQUITTO)
            message(STATUS "Using mosquitto via pkg-config (darwin): PkgConfig::MOSQUITTO")
        endif()
    endif()

    if(NOT MOSQ_FOUND)
        # Homebrew default prefixes (Apple Silicon + Intel)
        find_library(MOSQUITTO_LIB NAMES mosquitto HINTS /opt/homebrew/lib /usr/local/lib)
        find_path(MOSQUITTO_INCLUDE_DIR NAMES mosquitto.h HINTS /opt/homebrew/include /usr/local/include)
        if(MOSQUITTO_LIB AND MOSQUITTO_INCLUDE_DIR)
            message(STATUS "Using mosquitto (fallback darwin): ${MOSQUITTO_LIB}")
            set(MOSQ_FOUND TRUE)
            set(MOSQUITTO_LIB_FALLBACK ${MOSQUITTO_LIB})
            set(MOSQUITTO_INCLUDE_DIR_FALLBACK ${MOSQUITTO_INCLUDE_DIR})
        endif()
    endif()

    if(NOT MOSQ_FOUND)
        message(FATAL_ERROR "libmosquitto not found on macOS.\n"
            "Install it and reconfigure:\n"
            "  brew install mosquitto\n"
            "If CLion can't see pkg-config files, set PKG_CONFIG_PATH to "
            "/opt/homebrew/lib/pkgconfig (Apple Silicon) or /usr/local/lib/pkgconfig (Intel) in your CMake profile."
        )
    endif()
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

    # ---- Mosquitto (MQTT) discovery for Linux ----
    set(MOSQ_FOUND FALSE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        # Debian/Ubuntu typically ship 'libmosquitto' pc-name; some distros use 'mosquitto'
        pkg_check_modules(MOSQUITTO QUIET IMPORTED_TARGET libmosquitto)
        if(NOT TARGET PkgConfig::MOSQUITTO)
            pkg_check_modules(MOSQUITTO QUIET IMPORTED_TARGET mosquitto)
        endif()
        if(TARGET PkgConfig::MOSQUITTO)
            set(MOSQ_FOUND TRUE)
            set(MOSQUITTO_LINK_TARGET PkgConfig::MOSQUITTO)
            message(STATUS "Using mosquitto via pkg-config (linux): PkgConfig::MOSQUITTO")
        endif()
    endif()

    if(NOT MOSQ_FOUND)
        # Fallback common multi-arch libdirs
        find_library(MOSQUITTO_LIB NAMES mosquitto libmosquitto
            HINTS /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/lib/arm-linux-gnueabihf /usr/lib /usr/local/lib
        )
        find_path(MOSQUITTO_INCLUDE_DIR NAMES mosquitto.h
            HINTS /usr/include /usr/local/include
        )
        if(MOSQUITTO_LIB AND MOSQUITTO_INCLUDE_DIR)
            message(STATUS "Using mosquitto (fallback linux): ${MOSQUITTO_LIB}")
            set(MOSQ_FOUND TRUE)
            set(MOSQUITTO_LIB_FALLBACK ${MOSQUITTO_LIB})
            set(MOSQUITTO_INCLUDE_DIR_FALLBACK ${MOSQUITTO_INCLUDE_DIR})
        endif()
    endif()

    if(NOT MOSQ_FOUND)
        message(FATAL_ERROR "libmosquitto not found on Linux.\n"
            "Install development headers and reconfigure. Examples:\n"
            "  Debian/Ubuntu:   sudo apt install libmosquitto-dev pkg-config\n"
            "  Fedora/RHEL:     sudo dnf install mosquitto-devel pkgconfig\n"
            "  Arch:            sudo pacman -S mosquitto pkgconf\n"
            "  openSUSE:        sudo zypper in libmosquitto-devel pkg-config")
    endif()
else ()
    message(FATAL_ERROR "Unsupported platform: ${ANTZ_PLATFORM}")
endif()

set(SRC_FILES
    src/main.cpp
    src/mqtt.h
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


# Darwin-only linking for mosquitto (variables prepared in the darwin block above)
if(ANTZ_PLATFORM STREQUAL "darwin")
    if(DEFINED MOSQUITTO_LINK_TARGET)
        target_link_libraries(ant_discovery PRIVATE ${MOSQUITTO_LINK_TARGET})
    elseif(DEFINED MOSQUITTO_LIB_FALLBACK)
        target_include_directories(ant_discovery PRIVATE ${MOSQUITTO_INCLUDE_DIR_FALLBACK})
        target_link_libraries(ant_discovery PRIVATE ${MOSQUITTO_LIB_FALLBACK})
    endif()
endif()

# Linux-only linking for mosquitto (variables prepared in the linux block above)
if(ANTZ_PLATFORM STREQUAL "linux")
    if(DEFINED MOSQUITTO_LINK_TARGET)
        target_link_libraries(ant_discovery PRIVATE ${MOSQUITTO_LINK_TARGET})
    elseif(DEFINED MOSQUITTO_LIB_FALLBACK)
        target_include_directories(ant_discovery PRIVATE ${MOSQUITTO_INCLUDE_DIR_FALLBACK})
        target_link_libraries(ant_discovery PRIVATE ${MOSQUITTO_LIB_FALLBACK})
    endif()
endif()

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