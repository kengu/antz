# core/CMakeLists.txt

add_library(antz_platform STATIC
    src/antz_hal.h
    src/antz_platform.h
    src/antz_platform.cpp
    src/antz_platform_logging.h
)

target_include_directories(antz_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(antz_platform PRIVATE antz_core)

target_compile_features(antz_platform PUBLIC cxx_std_17)