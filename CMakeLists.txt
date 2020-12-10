cmake_minimum_required(VERSION 3.0.0)

cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0013 NEW)

set(PROJECT DXFeedAll)

option(DISABLE_TLS "Use ON value to disable TLS support" OFF)
option(BUILD_STATIC_LIBS "Use ON value to build DXFeed as a static library" OFF)

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
    set(PACKAGE_SUFFIX "-static")
else ()
    if (NOT DISABLE_TLS)
        set(PACKAGE_SUFFIX "")
    else (NOT DISABLE_TLS)
        set(PACKAGE_SUFFIX "-no-tls")
    endif (NOT DISABLE_TLS)
endif ()

#Note: add projects to build and package below
add_subdirectory(src)

add_subdirectory(tests/CommandLineSample)

add_subdirectory(tests/CandleSample)
add_subdirectory(tests/PriceLevelBookSample)
add_subdirectory(tests/RegionalBookSample)
add_subdirectory(tests/SnapshotConsoleSample)
add_subdirectory(tests/IncSnapshotConsoleSample)

add_subdirectory(wrappers/cpp)

add_subdirectory(tests/PerformanceTest)
add_subdirectory(tests/FullTest)
add_subdirectory(tests/APITest)
add_subdirectory(tests/ConnectionTest)

if (NOT BUILD_STATIC_LIBS)
    add_subdirectory(tests/LastEventTest)

    add_subdirectory(tests/QuoteTableTest)
    add_subdirectory(tests/SampleTest)
endif ()

set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
set(CPACK_PACKAGE_VENDOR "Devexperts LLC")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}${PACKAGE_SUFFIX}")
include(CPack)
