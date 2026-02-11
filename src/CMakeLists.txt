# Interface library for configuration
# Users must provide ZG_Config.h in their include path when integrating this library
# Users can set ZG_CONFIG_INCLUDE_DIR to specify where their ZG_Config.h is located
add_library(ZoneGaugeConfig INTERFACE)

if(DEFINED ZG_CONFIG_INCLUDE_DIR)
    target_include_directories(ZoneGaugeConfig INTERFACE
        ${ZG_CONFIG_INCLUDE_DIR}
    )
endif()

# Main library (header-only template library)
add_library(ZoneGauge INTERFACE)

target_include_directories(ZoneGauge INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ZoneGauge INTERFACE
    ZoneGaugeConfig
)

# Add source files to the interface for IDEs and include path
target_sources(ZoneGauge INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ZoneGauge.hpp
)

# Set C++20 standard for the library
target_compile_features(ZoneGauge INTERFACE cxx_std_20)
