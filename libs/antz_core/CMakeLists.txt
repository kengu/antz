# core/CMakeLists.txt

add_library(antz_core STATIC
        src/ant_channel.cpp
        src/ant_event.cpp
        src/ant_page.cpp
        src/profiles/hrm_decoder.cpp
#    profiles/asset_tracker/tracker_decoder.cpp
#    events/device_state.cpp
#    events/event_dispatcher.cpp
#    simulators/hrm_simulator.cpp
#    simulators/tracker_simulator.cpp
)

target_include_directories(antz_core PUBLIC
        .
)

target_compile_features(antz_core PUBLIC cxx_std_17)
