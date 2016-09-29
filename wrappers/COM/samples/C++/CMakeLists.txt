cmake_minimum_required (VERSION 2.6)

cmake_policy(SET CMP0015 NEW)

# Build on windows only
if(NOT WIN32)
    message(FATAL_ERROR "Not Windows!")
endif()
 
set(PROJECT DXFeedCOMC++Sample) 
set(LIB_COM_INCLUDE_DIR ../../include)
set(LIB_DXFEED_INCLUDE_DIR ../../../../include)
set(INCLUDE_DIR 
    ${LIB_DXFEED_INCLUDE_DIR}
    ${LIB_COM_INCLUDE_DIR}
)
set(TARGET_PLATFORM "x86" CACHE STRING "Target platform specification")
set(PLATFORM_POSTFIX "")
if(TARGET_PLATFORM STREQUAL "x64")
    set(PLATFORM_POSTFIX "_64")
endif()
set(DEBUG_POSTFIX "d${PLATFORM_POSTFIX}")
set(RELEASE_POSTFIX ${PLATFORM_POSTFIX})

set(LIB_DXFEED_PROJ DXFeed)
set(LIB_COM_SRC_DIR ../../src)
set(LIB_COM_PROJ DXFeedCOM)
set(LIB_COM_NAME ${LIB_COM_PROJ})
set(LIB_COM_OUT_DIR ${CMAKE_BINARY_DIR}/${LIB_COM_PROJ})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)
 
project(${PROJECT}) 

include_directories(${INCLUDE_DIR})

add_subdirectory(${LIB_COM_SRC_DIR} ${LIB_COM_OUT_DIR})

link_directories(${LIB_COM_OUT_DIR})

set(IMPORT_HEADERS
    ${LIB_COM_INCLUDE_DIR}/Guids.h
    ${LIB_COM_INCLUDE_DIR}/Interfaces.h
)

set(HEADER_FILES
    Sinks.h
    TypeLibraryManager.h
)

set(ALL_HEADERS
    ${IMPORT_HEADERS}
    ${HEADER_FILES}
)
 
set(SOURCE_FILES
    DXFeedCOMC++Sample.cpp
    Sinks.cpp
    TypeLibraryManager.cpp
)

set(CMAKE_CXX_FLAGS_DEBUG "/RTC1 /MTd /Od -D_DEBUG /ZI")
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MT -DNDEBUG /Zi")
add_definitions(-D_WIN32_WINNT=0x0500 -D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE /Gd /Zc:wchar_t /Zc:forScope /Gm- /W3)

set(ADDITIONAL_PROPERTIES
    ${ADDITIONAL_PROPERTIES}
    /SUBSYSTEM:CONSOLE
)

# Hack for remove standard libraries from linking
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
# End hack

source_group("Import headers" FILES ${IMPORT_HEADERS}) 
source_group("Header Files" FILES ${HEADER_FILES}) 
source_group("Source Files" FILES ${SOURCE_FILES}) 

add_executable(${PROJECT} ${ALL_HEADERS} ${SOURCE_FILES})

target_link_libraries(${PROJECT} 
    debug ${LIB_COM_NAME}${DEBUG_POSTFIX} 
    optimized ${LIB_COM_NAME}${RELEASE_POSTFIX}
    )

set_target_properties(${PROJECT} 
    PROPERTIES 
    DEBUG_POSTFIX "${DEBUG_POSTFIX}"
    RELEASE_POSTFIX "${RELEASE_POSTFIX}"
    LINK_FLAGS "${ADDITIONAL_PROPERTIES}"
)

set_target_properties(${LIB_DXFEED_PROJ} 
    PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)
set_target_properties(${LIB_COM_PROJ} 
    PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)

add_dependencies(${PROJECT} ${LIB_COM_PROJ})

set(LIB_COM_OBJ_DIR ${CMAKE_BINARY_DIR}/${LIB_COM_PROJ})
ADD_CUSTOM_COMMAND(
    TARGET ${PROJECT}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${LIB_COM_OBJ_DIR}/${LIB_COM_PROJ}.tlb ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${LIB_COM_OBJ_DIR}/${LIB_COM_PROJ}.h ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${LIB_COM_OBJ_DIR}/${LIB_COM_PROJ}_i.c ${CMAKE_BINARY_DIR}
)