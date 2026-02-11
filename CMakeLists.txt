cmake_minimum_required(VERSION 3.14)
project(ZoneGauge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTING "Build the testing tree" OFF)

# Force Google Test to use shared CRT to match our project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add the core library
add_subdirectory(src)

# Add tests if enabled
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(external/googletest)
    add_subdirectory(test)
endif()
