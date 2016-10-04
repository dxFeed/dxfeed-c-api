cmake_minimum_required (VERSION 2.8.1)

cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0013 NEW)

set(PROJECT DXFeedAll)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)

project(${PROJECT})
link_directories(${CMAKE_BINARY_DIR})

add_subdirectory(src)
add_subdirectory(tests/APITest)
add_subdirectory(tests/CandleSample)
add_subdirectory(tests/CommandLineSample)
add_subdirectory(tests/ConnectionTest)
add_subdirectory(tests/FullTest)
add_subdirectory(tests/LastEventTest)
add_subdirectory(tests/PerformanceTest)
add_subdirectory(tests/QuoteTableTest)
add_subdirectory(tests/SampleTest)
add_subdirectory(tests/SnapshotConsoleSample)
add_subdirectory(tests/UnitTests)
add_subdirectory(wrappers/COM/src)
add_subdirectory(wrappers/COM/samples/C++)


set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)