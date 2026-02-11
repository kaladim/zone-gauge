add_executable(ZoneGaugeTest
    ZoneGaugeTest.cpp
)

target_link_libraries(ZoneGaugeTest
    ZoneGauge
    gtest
    gtest_main
)

# Provide ZG_Config.h from test directory and access to ZoneGauge.h
target_include_directories(ZoneGaugeTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # For test's ZG_Config.h
    ${CMAKE_SOURCE_DIR}/src      # For ZoneGauge.h
)

# Add test to CTest
add_test(NAME ZoneGaugeTest COMMAND ZoneGaugeTest)
