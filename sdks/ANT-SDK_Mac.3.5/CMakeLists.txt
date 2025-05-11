cmake_minimum_required(VERSION 3.16)
project(ANT_MacOSX_Library C CXX)

add_definitions(-DDEBUG_FILE)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_BUILD_TYPE Debug)

# Include directories
include_directories(
    ANT_LIB
    ANT_LIB/common
    ANT_LIB/inc
    ANT_LIB/software
    ANT_LIB/software/ANTFS
    ANT_LIB/software/serial
    ANT_LIB/software/serial/device_management
    ANT_LIB/software/system
    ANT_LIB/software/USB
    ANT_LIB/software/USB/devices
    ANT_LIB/software/USB/device_handles
    ANT_LIB/software/USB/iokit_driver
)

# Global defines
add_compile_definitions(
    _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG
    _GLIBCXX_DEBUG=1
    _GLIBCXX_DEBUG_PEDANTIC=1
    DEBUG_FILE
)

# Separate C and C++ sources
file(GLOB_RECURSE ANTBASE_CPP_SRC
    ANT_LIB/*.cpp
    ANT_LIB/*/*.cpp
    ANT_LIB/*/*/*.cpp
)

set(ANTBASE_C_SRC
    ANT_LIB/common/crc.c
    ANT_LIB/common/checksum.c
    ANT_LIB/software/system/dsi_convert.c
    ANT_LIB/software/system/dsi_thread_posix.c
    ANT_LIB/software/system/macros.c
)

# macOS frameworks
find_library(IOKIT_FRAMEWORK IOKit)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)

# System libraries needed for POSIX/thread functions
find_library(SYSTEM_C_LIBRARY c)

# Static library: libantbase.a
add_library(antbase STATIC ${ANTBASE_C_SRC} ${ANTBASE_CPP_SRC})
target_link_libraries(antbase PUBLIC ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK} ${SYSTEM_C_LIBRARY})
target_include_directories(antbase PUBLIC
    ANT_LIB
    ANT_LIB/common
    ANT_LIB/inc
)


# Shared library: libantbase_shared.dylib
add_library(antbase_shared SHARED ${ANTBASE_CPP_SRC} ${ANTBASE_C_SRC})
target_link_libraries(antbase_shared
    PUBLIC
        antbase
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${SYSTEM_C_LIBRARY}
)

set_target_properties(antbase_shared PROPERTIES
    INSTALL_NAME_DIR "@executable_path"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Executable demo_lib (links statically)
add_executable(demo_lib DEMO_LIB/demo.cpp)
target_include_directories(demo_lib PRIVATE DEMO_LIB)
target_link_libraries(demo_lib PRIVATE antbase)

# Executable demo_dylib (links dynamically)
add_executable(demo_dylib DEMO_LIB/demo.cpp)
target_include_directories(demo_dylib PRIVATE DEMO_LIB)
target_link_libraries(demo_dylib PRIVATE antbase_shared)

add_custom_command(TARGET demo_dylib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:antbase_shared>
        $<TARGET_FILE_DIR:demo_dylib>
    COMMAND install_name_tool -change
        $<TARGET_FILE:antbase_shared>
        @executable_path/$<TARGET_FILE_NAME:antbase_shared>
        $<TARGET_FILE:demo_dylib>
    COMMENT "Patching demo_dylib to use local antbase_shared dylib"
)

# Shared library libant -> libant.dylib
add_library(libant SHARED DEMO_DLL/libant.cpp)
set_target_properties(libant PROPERTIES
    OUTPUT_NAME ant
    INSTALL_NAME_DIR "@executable_path"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Executable demo_dll
add_executable(demo_dll DEMO_DLL/demo.cpp)
target_include_directories(demo_dll PRIVATE DEMO_DLL)
target_link_libraries(demo_dll PRIVATE libant)

add_custom_command(TARGET demo_dll POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:libant>
        $<TARGET_FILE_DIR:demo_dll>
    COMMAND install_name_tool -change
        $<TARGET_FILE:libant>
        @executable_path/$<TARGET_FILE_NAME:libant>
        $<TARGET_FILE:demo_dll>
    COMMENT "Patching demo_dll to use local libant.dylib"
)

# Custom build targets
add_custom_target(static_build DEPENDS DEMO_LIB)
add_custom_target(shared_build DEPENDS demo_dylib)
add_custom_target(full_build
    DEPENDS
        static_build
        shared_build
        DEMO_DLL
)

# Install target to ./Bin
include(GNUInstallDirs)
install(DIRECTORY ANT_LIB
        DESTINATION Bin/include
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(TARGETS antbase antbase_shared libant demo_lib demo_dylib demo_dll
        RUNTIME DESTINATION Bin
        LIBRARY DESTINATION Bin
        ARCHIVE DESTINATION Bin)

add_custom_target(install_local
    COMMAND ${CMAKE_COMMAND} --install . --prefix Bin
    COMMENT "Installing to ./Bin")
