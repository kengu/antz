project(ant_discovery)

set(CMAKE_BUILD_TYPE Debug)
add_definitions(-DDEBUG_FILE)

add_executable(ant_discovery
        src/main.cpp
        src/discovery.cpp
        src/logging.h
        src/ant_device.h
        src/ant_profiles.h
        src/hrm_discovery.cpp
        src/asset_tracker_discovery.cpp
        src/ant_constants.h
)

# Kun lokale includes og SDK-headers fra installert Bin
# SDK-headere fra antbase eksponeres via target_link_libraries

target_include_directories(ant_discovery PRIVATE
        include
        src
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/inc
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/common
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/ANTFS
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/serial
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/serial/device_management
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/system
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/USB
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/USB/devices
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/USB/device_handles
        ${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin/include/software/USB/iokit_driver
)

link_directories(${CMAKE_SOURCE_DIR}/ANT-SDK_Mac.3.5/Bin)
target_link_libraries(ant_discovery PRIVATE antbase)

set_target_properties(ant_discovery PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Bin
)

message(STATUS "Output path: ${CMAKE_CURRENT_SOURCE_DIR}/Bin")
