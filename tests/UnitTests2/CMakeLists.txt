cmake_minimum_required(VERSION 3.0.0)

cmake_policy(SET CMP0015 NEW)

project(UnitTests2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

include_directories(../../include
        thirdparty/catch2
        )
