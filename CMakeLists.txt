cmake_minimum_required (VERSION 4.1)
project(home_automation)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

set (CXX_COMMON_FLAGS "-std=c++17 -Wall -Wextra -Werror")
set (CMAKE_CXX_FLAGS_DEBUG "${CXX_COMMON_FLAGS} -O0 -g")
set (CMAKE_CXX_FLAGS_RELEASE "${CXX_COMMON_FLAGS} -O2")

include_directories(src)
include_directories(test/ArduinoJson)
include_directories(test/googletest/googlemock/include)

add_subdirectory(test/googletest)

file(GLOB test_sources test/*.cpp)
file(GLOB common_sources src/common/*.cpp)
file(GLOB operation_sources src/operation/*.cpp)
file(GLOB tools_sources src/tools/*.cpp)

add_executable(home_automation_test
    ${test_sources} ${operation_sources} ${common_sources} ${tools_sources})
target_link_libraries(home_automation_test gtest gtest_main)

add_executable(operation_tester
    ${operation_sources}  ${common_sources} ${tools_sources}
    bin/operation_tester.cpp)
