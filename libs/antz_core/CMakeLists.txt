# core/CMakeLists.txt

add_library(antz_core STATIC
    src/antz_core_logging.h
    src/data/antz_data.h
    src/data/antz_data.cpp
    src/event/antz_event.h
    src/event/antz_event.cpp
    src/page/antz_page.h
    src/page/antz_page.cpp
    src/profiles/antz_profile.h
    src/profiles/antz_profile.cpp
    src/profiles/hrm/hrm_decoder.h
    src/profiles/hrm/hrm_decoder.cpp
    src/logger/antz_logger.h
    src/logger/antz_logger.cpp
    src/error/antz_error.h
    src/error/antz_error.cpp
    src/logger/antz_xmacro_logger.h
)

target_include_directories(antz_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(antz_core PUBLIC cxx_std_17)