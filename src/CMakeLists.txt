# Interface library for configuration
# Users must provide ZG_Config.h in their include path when integrating this library
# Users can set ZG_CONFIG_INCLUDE_DIR to specify where their ZG_Config.h is located
add_library(ZoneGaugeConfig INTERFACE)

if(DEFINED ZG_CONFIG_INCLUDE_DIR)
    target_include_directories(ZoneGaugeConfig INTERFACE
        ${ZG_CONFIG_INCLUDE_DIR}
    )
endif()

# Main library
add_library(ZoneGauge STATIC
    ZoneGauge.c
)

target_include_directories(ZoneGauge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ZoneGauge PUBLIC
    ZoneGaugeConfig
)

# Set C standard for the library
set_target_properties(ZoneGauge PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
