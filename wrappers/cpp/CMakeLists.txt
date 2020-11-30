cmake_minimum_required(VERSION 3.0.0)

cmake_policy(SET CMP0015 NEW)

project(DXFeedCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

include_directories(../../include
    thirdparty/optional-lite/include
    thirdparty/string-view-lite/include
    thirdparty/variant-lite/include
    thirdparty/fmt/include
    thirdparty/args
    )

add_definitions(-DFMT_HEADER_ONLY=1)

set(TARGET_PLATFORM "x86" CACHE STRING "Target platform specification")
set(PLATFORM_POSTFIX "")
if(TARGET_PLATFORM STREQUAL "x64")
    set(PLATFORM_POSTFIX "_64")
endif()
set(DEBUG_POSTFIX "d${PLATFORM_POSTFIX}")
set(RELEASE_POSTFIX ${PLATFORM_POSTFIX})
set(LIB_TLS ../../lib/libressl)
set(LIB_DXFEED_SRC_DIR ../../src)
set(LIB_DXFEED_PROJ DXFeed)
set(LIB_DXFEED_NAME ${LIB_DXFEED_PROJ})
set(LIB_DXFEED_OUT_DIR ${CMAKE_BINARY_DIR}/${LIB_DXFEED_PROJ})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE INTERNAL "" FORCE)

add_subdirectory(src)
add_subdirectory(samples)

set(BUILD_FILES CMakeLists.txt)
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")

install(DIRECTORY thirdparty DESTINATION "wrappers/cpp" CONFIGURATIONS Release)
install(FILES ${BUILD_FILES} DESTINATION "wrappers/cpp" CONFIGURATIONS Release)

set(CPACK_PACKAGE_VENDOR "dxFeed Solutions DE GmbH")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)

