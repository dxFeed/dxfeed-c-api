cmake_minimum_required (VERSION 2.8.1)

cmake_policy(SET CMP0015 NEW)

# Build on windows only
if(NOT WIN32)
    message(FATAL_ERROR "Not Windows!")
endif()
 
set(PROJECT DXFeedCOM) 
set(LIB_COM_INCLUDE_DIR ../include)
set(LIB_DXFEED_INCLUDE_DIR ../../../include)
set(INCLUDE_DIR 
    ${LIB_DXFEED_INCLUDE_DIR}
    ${LIB_COM_INCLUDE_DIR}
)
set(TARGET_PLATFORM "x86" CACHE STRING "Target platform specification")
set(APP_VERSION "" CACHE STRING "The version of library")
set(C_API_VERSION_MAJOR 0)
set(C_API_VERSION_MINOR 0)
set(C_API_VERSION_PATCH 0)
set(POSTFIX "")
set(PLATFORM_POSTFIX "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(POSTFIX "d")
endif()
if(TARGET_PLATFORM STREQUAL "x64")
    set(PLATFORM_POSTFIX "_64")
    set(POSTFIX "${POSTFIX}${PLATFORM_POSTFIX}")
endif()

set(DEF_FILE ${PROJECT}${POSTFIX}.def)
set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}.idl)
set(LIB_DXFEED_SRC_DIR ../../../src)
set(LIB_DXFEED_PROJ DXFeed)
set(LIB_DXFEED_NAME ${LIB_DXFEED_PROJ}${POSTFIX})
set(LIB_DXFEED_OUT_DIR ${CMAKE_BINARY_DIR}/${LIB_DXFEED_PROJ})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)
set(CMAKE_DEBUG_POSTFIX "d${PLATFORM_POSTFIX}")
set(CMAKE_RELEASE_POSTFIX "${PLATFORM_POSTFIX}")
 
project(${PROJECT})

# If library version is not specified through parameters version from the source will be used
if(NOT ${APP_VERSION} STREQUAL "")

    #separate version string on components
    string(REPLACE "." ";" VERSION_COMPONENTS_LIST ${APP_VERSION})
    list(LENGTH VERSION_COMPONENTS_LIST VERSION_COMPONENTS_COUNT)
    list(GET VERSION_COMPONENTS_LIST 0 C_API_VERSION_MAJOR)
    if(VERSION_COMPONENTS_COUNT GREATER 1)
        list(GET VERSION_COMPONENTS_LIST 1 C_API_VERSION_MINOR)
    endif()
    if(VERSION_COMPONENTS_COUNT GREATER 2)
        list(GET VERSION_COMPONENTS_LIST 2 C_API_VERSION_PATCH)
    endif()
    
    configure_file("${PROJECT_SOURCE_DIR}/Version.h.in" "${PROJECT_SOURCE_DIR}/Version.h")
    configure_file("${PROJECT_SOURCE_DIR}/${DEF_FILE}.in" "${PROJECT_SOURCE_DIR}/${DEF_FILE}")
endif()

include_directories(${INCLUDE_DIR})

if (NOT TARGET ${LIB_DXFEED_PROJ})
    add_subdirectory(${LIB_DXFEED_SRC_DIR} ${LIB_DXFEED_OUT_DIR})
endif()

link_directories(${LIB_DXFEED_OUT_DIR})

set(EXTERNAL_HEADERS
    ${LIB_COM_INCLUDE_DIR}/Guids.h
    ${LIB_COM_INCLUDE_DIR}/Interfaces.h
)

set(HEADER_FILES
    AuxAlgo.h
    CandleSymbol.h
    Connection.h
    ConnectionPointImpl.h
    DispatchImpl.h
    EventFactory.h
    Feed.h
    LibraryLocker.h
    NativeCandleSymbol.h
    resource.h
    Snapshot.h
    Subscription.h
    TypeLibraryManager.h
    Version.h
)

set(ALL_HEADERS
    ${EXTERNAL_HEADERS}
    ${HEADER_FILES}
)
 
set(SOURCE_FILES
    AuxAlgo.cpp
    CandleSymbol.cpp
    ClassFactory.cpp
    Connection.cpp
    ConnectionPointImpl.cpp
    DispatchImpl.cpp
    DXFeedCOM.cpp
    EventFactory.cpp
    Feed.cpp
    LibraryLocker.cpp
    NativeCandleSymbol.cpp
    SelfRegistration.cpp
    Snapshot.cpp
    Subscription.cpp
    TypeLibraryManager.cpp
    ${IDL_FILE}
    ${DEF_FILE}
)

set (RESOURCE_FILES
    DXFeedCOM.rc
    )

set (ALL_SOURCES
    ${SOURCE_FILES}
    ${RESOURCE_FILES}
    )

set(CMAKE_CXX_FLAGS_DEBUG "/RTC1 /MTd /Od -D_DEBUG /Zi")
set(CMAKE_CXX_FLAGS_RELEASE "/Ox /MT -DNDEBUG")
add_definitions(-D_WIN32_WINNT=0x0500 -D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE /Gd /Zc:wchar_t /Zc:forScope /Gm- /W3)

set(ADDITIONAL_PROPERTIES
    ${ADDITIONAL_PROPERTIES}
    /SUBSYSTEM:WINDOWS
)

# Hack for remove standard libraries from linking
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
# End hack

source_group("External interface" FILES ${EXTERNAL_HEADERS}) 
source_group("Header Files" FILES ${HEADER_FILES}) 
source_group("Source Files" FILES ${SOURCE_FILES}) 

add_library(${PROJECT} SHARED ${ALL_HEADERS} ${ALL_SOURCES})

target_link_libraries(${PROJECT} ${LIB_DXFEED_NAME} comsupp.lib Advapi32.lib)

set_target_properties(${PROJECT} 
    PROPERTIES 
    LINK_FLAGS "${ADDITIONAL_PROPERTIES}"
)

add_dependencies(${PROJECT} ${LIB_DXFEED_PROJ})

message("Run MIDL compiller...")
set(MIDL_H_FILE ${PROJECT}.h)
set(MIDL_C_FILE ${PROJECT}_i.c)
set(MIDL_TLB_FILE ${PROJECT}.tlb)
add_custom_command(
    TARGET ${PROJECT}
    POST_BUILD
    COMMAND midl /env win32 /h "${MIDL_H_FILE}" /W1 /char signed /tlb "${MIDL_TLB_FILE}" "${IDL_FILE}"
)

set(TLB_OUT_DIR ${CMAKE_BINARY_DIR}/$(Configuration))
ADD_CUSTOM_COMMAND(
    TARGET ${PROJECT}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${MIDL_TLB_FILE} ${TLB_OUT_DIR}/${MIDL_TLB_FILE}
    COMMAND ${CMAKE_COMMAND} -E copy ${MIDL_H_FILE} ${TLB_OUT_DIR}/${MIDL_H_FILE}
    COMMAND ${CMAKE_COMMAND} -E copy ${MIDL_C_FILE} ${TLB_OUT_DIR}/${MIDL_C_FILE}
)


set(PROJECT_FILES
    ${PROJECT}.vcxproj
    ${PROJECT}.vcxproj.filters
)
set(BUILD_FILES
    build.bat
    CMakeLists.txt
)
set(ALL_DEF_FILES
    ${PROJECT}.def
    ${PROJECT}d.def
    ${PROJECT}_64.def
    ${PROJECT}d_64.def
)
set(ALL_DEF_TEMPLATES
    ${PROJECT}.def.in
    ${PROJECT}d.def.in
    ${PROJECT}_64.def.in
    ${PROJECT}d_64.def.in
)
set(ALL_VERSION_TEMPLATES
    Version.h.in
)
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
install(TARGETS ${PROJECT}
    DESTINATION "bin/${TARGET_PLATFORM}"
)
install(DIRECTORY "${LIB_COM_INCLUDE_DIR}"
    DESTINATION "wrappers/COM"
)
install(FILES ${HEADER_FILES} ${ALL_SOURCES} ${ALL_DEF_FILES} ${PROJECT_FILES} ${BUILD_FILES} ${ALL_DEF_TEMPLATES} ${ALL_VERSION_TEMPLATES}
    DESTINATION "wrappers/COM/src"
)
install(FILES "${PROJECT_BINARY_DIR}/DXFeedCOM.tlb" "${PROJECT_BINARY_DIR}/DXFeedCOM.h" "${PROJECT_BINARY_DIR}/DXFeedCOM_i.c"
    DESTINATION "bin/${TARGET_PLATFORM}/tlb"
)
set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)
