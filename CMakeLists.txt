cmake_minimum_required (VERSION 3.2)
project(home_automation)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

set (CXX_COMMON_FLAGS "-std=c++11 -Wall -Wextra")
set (CMAKE_CXX_FLAGS_DEBUG "${CXX_COMMON_FLAGS} -O0 -g")
set (CMAKE_CXX_FLAGS_RELEASE "${CXX_COMMON_FLAGS} -O2")
find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_libraries(boost_unit_test_framework)

include_directories(src)

file(GLOB test_sources test/*.cpp)
file(GLOB common_sources src/common/*.cpp)
file(GLOB tools_sources src/tools/*.cpp)

add_executable(home_automation_test
    ${test_sources} ${common_sources} ${tools_sources})
