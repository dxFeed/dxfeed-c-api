cmake_minimum_required (VERSION 2.6)

cmake_policy(SET CMP0015 NEW)
 
set(PROJECT DXFeed) 
set(INCLUDE_DIR ../include)
set(PTHREADS_INCLUDE_DIR ../include/pthreads)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(POSTFIX "d")
else()
    set(POSTFIX "")
endif()
set(DEF_FILE ../${PROJECT}${POSTFIX}.def)

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)
 
project(${PROJECT}) 

include_directories(${INCLUDE_DIR})

set(EXPORT_HEADERS
    ${INCLUDE_DIR}/DXErrorCodes.h
    ${INCLUDE_DIR}/DXFeed.h
    ${INCLUDE_DIR}/DXTypes.h
    ${INCLUDE_DIR}/EventData.h
    ${INCLUDE_DIR}/RecordData.h
    )
 
set (HEADER_FILES 
    ClientMessageProcessor.h
    ConnectionContextData.h
    DXAlgorithms.h
    DXErrorHandling.h
    DXMemory.h
    DXNetwork.h
    DXPMessageData.h
    DXSockets.h
    DXThreads.h
    EventSubscription.h
    Logger.h
    PrimitiveTypes.h
    RecordTranscoder.h
    ServerMessageProcessor.h
    TaskQueue.h
    )
 
 set (PARSER_HEADERS
    BufferedInput.h
    BufferedIOCommon.h
    BufferedOutput.h
    DataStructures.h
    Decimal.h
    RecordBuffers.h
    RecordFieldSetters.h
    SymbolCodec.h
    )
    
set (PARSER_SOURCES
    BufferedInput.c
    BufferedIOCommon.c
    BufferedOutput.c
    DataStructures.c
    Decimal.c
    RecordBuffers.c
    RecordFieldSetters.c
    SymbolCodec.c
    )
    
set (PARSER_TESTS
    TestParser.c
)
 
set (SOURCE_FILES
    ClientMessageProcessor.c
    ConnectionContextData.c
    DXAlgorithms.c
    DXErrorCodes.c
    DXErrorHandling.c
    DXFeed.c
    DXMemory.c
    DXNetwork.c
    DXPMessageData.c
    DXSockets.c
    DXThreads.c
    EventData.c
    EventSubscription.c
    Logger.c
    RecordTranscoder.c
    ServerMessageProcessor.c
    TaskQueue.c
    Win32.c
    ${DEF_FILE}) 
    
set (ALL_HEADERS
    ${HEADER_FILES}
    ${EXPORT_HEADERS}
    ${PARSER_HEADERS}
    )
    
set (ALL_SOURCES
    ${SOURCE_FILES}
    ${PARSER_SOURCES}
    ${PARSER_TESTS}
    )
    
set (ADDITIONAL_LIBRARIES "")

if (MSVC)
    set(CMAKE_C_FLAGS_DEBUG "/TC /RTC1 /MTd /Od")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /MT")
    add_definitions (-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_USRDLL -DDXFEED_EXPORTS /Gd /TC /Zc:wchar_t /Zc:forScope /Gm- /W3 /Ob0)
    
    set (ADDITIONAL_LIBRARIES
        ${ADDITIONAL_LIBRARIES}
        ws2_32.lib
    )
    
    # Hack for remove standard libraries from linking
    set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
    # End hack
    
    include_directories(${PTHREADS_INCLUDE_DIR})
    
    set (PTHREAD_HEADERS
        ${PTHREADS_INCLUDE_DIR}/pthread.h
        ${PTHREADS_INCLUDE_DIR}/sched.h
        ${PTHREADS_INCLUDE_DIR}/semaphore.h
    )
    
    set (ALL_HEADERS
        ${ALL_HEADERS}
        ${PTHREAD_HEADERS}
    )
    
    source_group("Third Party\\pthreads" FILES ${PTHREAD_HEADERS})
    
elseif (CMAKE_COMPILER_IS_GNUCC)
	add_definitions (-O2 -DUSE_PTHREADS -std=gnu99 -fPIC)
    
    set (SOURCE_FILES
        ${SOURCE_FILES}
        Linux.c
    )
    set (ALL_SOURCES
        ${ALL_SOURCES}
        Linux.c
    )
    set (ADDITIONAL_LIBRARIES
        ${ADDITIONAL_LIBRARIES}
        pthread
    )
    
else ()
	message ("Unknown compiler")
endif ()

source_group ("Export" FILES ${EXPORT_HEADERS})    
source_group ("Header Files" FILES ${HEADER_FILES})
source_group ("Parser\\Headers" FILES ${PARSER_HEADERS})
source_group ("Parser\\Sources" FILES ${PARSER_SOURCES})
source_group ("Parser\\Test" FILES ${PARSER_TESTS})
source_group ("Source Files" FILES ${SOURCE_FILES}) 

add_library (${PROJECT} SHARED ${ALL_HEADERS} ${ALL_SOURCES})

target_link_libraries (${PROJECT} ${ADDITIONAL_LIBRARIES})

set_target_properties(${PROJECT} PROPERTIES DEBUG_POSTFIX "d")
