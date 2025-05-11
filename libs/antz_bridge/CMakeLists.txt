# bridge/CMakeLists.txt

add_library(antz_bridge STATIC
        src/ant_bridge.cpp
#    ble_output.cpp
)

target_include_directories(antz_bridge PUBLIC
        .
)

target_link_libraries(antz_bridge PUBLIC antz_core)

target_compile_features(antz_bridge PUBLIC cxx_std_17)
