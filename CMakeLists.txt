cmake_minimum_required(VERSION 3.0.0)

cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0013 NEW)

set(PROJECT DXFeedAll)

option(DISABLE_TLS "Use ON value to disable TLS support" OFF)
option(BUILD_STATIC_LIBS "Use ON value to build dxFeed framework as a static library" OFF)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE INTERNAL "" FORCE)

project(${PROJECT})
link_directories(${CMAKE_BINARY_DIR})

if (WIN32 OR MACOS)
    set(BUILD_STATIC_LIBS OFF)
endif()

if (BUILD_STATIC_LIBS)
    set(DISABLE_TLS ON)
    set(PACKAGE_SUFFIX "-static-no-tls")
else ()
    if (DISABLE_TLS)
        set(PACKAGE_SUFFIX "-no-tls")
    else (NOT DISABLE_TLS)
        set(PACKAGE_SUFFIX "")
    endif (DISABLE_TLS)
endif (BUILD_STATIC_LIBS)

add_subdirectory(thirdparty/toml11)

#Note: add projects to build and package below
add_subdirectory(src)

add_subdirectory(samples/CandleSample)
add_subdirectory(samples/CommandLineSample)
add_subdirectory(samples/FullOrderBookSample)
add_subdirectory(samples/PriceLevelBookSample)
add_subdirectory(samples/PriceLevelBookSample2)
add_subdirectory(samples/RegionalBookSample)
add_subdirectory(samples/SnapshotConsoleSample)
add_subdirectory(samples/IncSnapshotConsoleSample)

add_subdirectory(wrappers/cpp)

add_subdirectory(tests/FullTest)

add_subdirectory(tests/APITest)
add_subdirectory(tests/ConnectionTest)
add_subdirectory(tests/PerformanceTest)
add_subdirectory(tests/LastEventTest)
add_subdirectory(tests/QuoteTableTest)
add_subdirectory(tests/SampleTest)

set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
set(CPACK_PACKAGE_VENDOR "Devexperts LLC")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}${PACKAGE_SUFFIX}")
include(CPack)
