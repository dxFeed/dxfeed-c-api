cmake_minimum_required (VERSION 2.6)

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
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(POSTFIX "d")
else()
    set(POSTFIX "")
endif()
set(DEF_FILE ${PROJECT}${POSTFIX}.def)
set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}.idl)
set(LIB_DXFEED_SRC_DIR ../../../src)
set(LIB_DXFEED_PROJ DXFeed)
set(LIB_DXFEED_NAME ${LIB_DXFEED_PROJ}${POSTFIX})
set(LIB_DXFEED_OUT_DIR ${CMAKE_BINARY_DIR}/${LIB_DXFEED_PROJ})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)
 
project(${PROJECT}) 

include_directories(${INCLUDE_DIR})

add_subdirectory(${LIB_DXFEED_SRC_DIR} ${LIB_DXFEED_OUT_DIR})

link_directories(${LIB_DXFEED_OUT_DIR})

set(EXTERNAL_HEADERS
    ${LIB_COM_INCLUDE_DIR}/Guids.h
    ${LIB_COM_INCLUDE_DIR}/Interfaces.h
)

set(HEADER_FILES
    AuxAlgo.h
    Connection.h
    ConnectionPointImpl.h
    DispatchImpl.h
    EventFactory.h
    Feed.h
    LibraryLocker.h
    Subscription.h
    TypeLibraryManager.h
)

set(ALL_HEADERS
    ${EXTERNAL_HEADERS}
    ${HEADER_FILES}
)
 
set(SOURCE_FILES
    AuxAlgo.cpp
    ClassFactory.cpp
    Connection.cpp
    ConnectionPointImpl.cpp
    DispatchImpl.cpp
    DXFeedCOM.cpp
    EventFactory.cpp
    Feed.cpp
    LibraryLocker.cpp
    SelfRegistration.cpp
    Subscription.cpp
    TypeLibraryManager.cpp
    ${IDL_FILE}
    ${DEF_FILE}
)

set(CMAKE_CXX_FLAGS_DEBUG "/RTC1 /MTd /Od -D_DEBUG /ZI")
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

add_library(${PROJECT} SHARED ${ALL_HEADERS} ${SOURCE_FILES})

target_link_libraries(${PROJECT} ${LIB_DXFEED_NAME} comsupp.lib Advapi32.lib)

set_target_properties(${PROJECT} 
    PROPERTIES 
    DEBUG_POSTFIX "d"
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
