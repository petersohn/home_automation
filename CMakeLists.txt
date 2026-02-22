cmake_minimum_required (VERSION 4.1)
project(home_automation)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

set (CXX_COMMON_FLAGS "-std=c++17 -Wall -Wextra")
set (CMAKE_CXX_FLAGS_DEBUG "${CXX_COMMON_FLAGS} -O0 -g")
set (CMAKE_CXX_FLAGS_RELEASE "${CXX_COMMON_FLAGS} -O2")
find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(src)
include_directories(test/ArduinoJson)

file(GLOB test_sources test/*.cpp)
file(GLOB common_sources src/common/*.cpp)
file(GLOB operation_sources src/operation/*.cpp)
file(GLOB tools_sources src/tools/*.cpp)

add_executable(home_automation_test
    ${test_sources} ${operation_sources} ${common_sources} ${tools_sources})
target_include_directories(home_automation_test PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(home_automation_test boost_unit_test_framework)

add_executable(operation_tester
    ${operation_sources}  ${common_sources} ${tools_sources}
    bin/operation_tester.cpp)
