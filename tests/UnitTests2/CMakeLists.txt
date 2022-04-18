cmake_minimum_required(VERSION 3.0.0)

cmake_policy(SET CMP0015 NEW)

project(DXFeedTests2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DXFC_TEST_SOURCES WideDecimalTest.cpp)
set(DXFC_INCLUDE_DIRS ../../include ../../src ../../thirdparty/Catch2-2.13.8/single_include)

string(REPLACE ".cpp" "" DXFC_TEST_BASENAMES "${DXFC_TEST_SOURCES}")
set(DXFC_TEST_TARGETS ${DXFC_TEST_BASENAMES})

add_library(DXFeedTests2Main OBJECT Main.cpp)
target_include_directories(DXFeedTests2Main PRIVATE ${DXFC_INCLUDE_DIRS})

add_executable(WideDecimalTest WideDecimalTest.cpp ../../src/WideDecimal.h ../../src/WideDecimal.hpp)

foreach(name ${DXFC_TEST_TARGETS})
    target_include_directories(${name} PRIVATE ${DXFC_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE DXFeedTests2Main)
    set_property(TARGET ${name} PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${name} PROPERTY CXX_EXTENSIONS OFF)
    add_test(NAME ${name} COMMAND ${name})
endforeach()
