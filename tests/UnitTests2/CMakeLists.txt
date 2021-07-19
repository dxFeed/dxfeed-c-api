cmake_minimum_required(VERSION 3.0.0)

project(UnitTests2)
cmake_policy(SET CMP0015 NEW)
set(PROJECT UnitTests2)

add_subdirectory(Catch2)

add_executable(${PROJECT} main.cpp)
target_link_libraries(${PROJECT} Catch2::Catch2)

include(CTest)
include(Catch2/contrib/Catch.cmake)
catch_discover_tests(${PROJECT})