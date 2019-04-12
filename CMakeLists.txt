cmake_minimum_required(VERSION 2.8.1)

cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0013 NEW)

set(PROJECT DXFeedAll)

option(DISABLE_TLS "Use ON value to disable TLS support" OFF)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)

project(${PROJECT})
link_directories(${CMAKE_BINARY_DIR})

if (NOT DISABLE_TLS)
    set(PACKAGE_SUFFIX "")
else (NOT DISABLE_TLS)
    set(PACKAGE_SUFFIX "-no-tls")
endif (NOT DISABLE_TLS)

#Note: add projects to build and package below
add_subdirectory(src)

add_subdirectory(tests/CandleSample)
add_subdirectory(tests/CommandLineSample)

if (WIN32)
    add_subdirectory(tests/FullTest)
endif (WIN32)

add_subdirectory(tests/PriceLevelBookSample)
add_subdirectory(tests/RegionalBookSample)
add_subdirectory(tests/SnapshotConsoleSample)
add_subdirectory(tests/IncSnapshotConsoleSample)

if (WIN32 AND MSVC)
    add_subdirectory(wrappers/COM/src)
    add_subdirectory(wrappers/COM/samples/C++)
endif (WIN32 AND MSVC)

#if (WIN32)
    add_subdirectory(wrappers/cpp)
#endif (WIN32)

set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}${PACKAGE_SUFFIX}")
include(CPack)