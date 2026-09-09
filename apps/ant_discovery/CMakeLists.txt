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

# The built application. discovery.cpp is the live decoder: main.cpp drives it
# directly, and everything the Debian package does goes through it.
set(SRC_FILES
    src/main.cpp
    src/mqtt.h
    src/usb_checker.cpp
    src/discovery.cpp
    src/ant_decode.h
    src/logging.h
    src/ant_device.h
    src/ant_profiles.h
    src/ant_constants.h
    src/config.h
)

# The DiscoveryMachine architecture that will replace the above. Deliberately
# not in SRC_FILES and not built:
#
#   src/main_new.cpp              entry point
#   src/discovery_machine.cpp/.h  channel and profile orchestration
#   src/profile_discovery.h       the per-profile interface
#   src/hrm_discovery.cpp         HRM profile   — complete
#   src/asset_tracker_discovery.cpp  Asset Tracker profile — a stub; the
#                                 working decoder is still discovery.cpp
#
# hrm_discovery.cpp and asset_tracker_discovery.cpp were in SRC_FILES and
# compiled into the shipped binary, where nothing instantiated either class.
# Their headers are still included by discovery.cpp, which uses the page
# constants they declare but neither class — so the headers stay and the
# translation units go. Moving those constants into ant_constants.h would let
# the profile headers become purely part of the new architecture; that is a
# change to the live decoder's include graph and is left for the migration.
#
# Because these files are unbuilt they are also unchecked, and will rot until
# the migration resumes.

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
# and set the binary name to 'antz' for installation
set_target_properties(ant_discovery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    OUTPUT_NAME antz
)

message(STATUS "ant_discovery output path: ${CMAKE_CURRENT_SOURCE_DIR}/bin")

# Install rules

install(TARGETS ant_discovery
    RUNTIME DESTINATION bin
    COMPONENT applications
)

# Install systemd service file
install(FILES ${CMAKE_SOURCE_DIR}/packaging/linux/conf/antz.service
    DESTINATION lib/systemd/system
    COMPONENT services
)

# Install udev rules for ANT+ USB dongles
install(FILES ${CMAKE_SOURCE_DIR}/packaging/linux/conf/99-antz.rules
    DESTINATION lib/udev/rules.d
    COMPONENT udev
)

# ── Tests ──────────────────────────────────────────────────────────────────
#
# ant_decode.h holds the profile bit-decoding and depends on nothing but the
# standard library, so these link neither the ANT SDK nor a dongle and run
# anywhere. They are not installed and not part of the Debian package.
#
#   ctest --test-dir <build> --output-on-failure

add_executable(antz_decode_test tests/decode_test.cpp)
target_include_directories(antz_decode_test PRIVATE src)
target_compile_features(antz_decode_test PRIVATE cxx_std_20)

# The fragment builders need the app's structs, so this one also needs the ANT
# SDK headers for its typedefs — headers only, nothing is linked.
add_executable(antz_format_test tests/format_test.cpp)
target_include_directories(antz_format_test PRIVATE src ${ANT_SDK_INCLUDE_DIRS})
target_compile_features(antz_format_test PRIVATE cxx_std_20)

# One CTest entry per group rather than one for the binary: a failure then
# names the part of the spec that broke, and a group can be run alone.
foreach(group
        tracker.status_byte
        tracker.asset_index
        tracker.units
        hrm.page_and_rate)
    add_test(NAME ${group} COMMAND antz_decode_test ${group})
endforeach()

# One target that builds every test executable, so CI does not name them
# individually — a workflow that builds one target and runs ctest reports a
# missing binary as "Not Run", which is a green-looking way to skip a suite.
add_custom_target(antz_tests DEPENDS antz_decode_test antz_format_test)

foreach(group
        format.checker
        format.hrm_json
        format.hrm_csv
        format.device_json
        format.device_csv
        format.escaping)
    add_test(NAME ${group} COMMAND antz_format_test ${group})
endforeach()
