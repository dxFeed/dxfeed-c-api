cmake_minimum_required (VERSION 2.8.1)

cmake_policy(SET CMP0015 NEW)
 
set(PROJECT DXFeed) 
set(INCLUDE_DIR ../include)
set(PTHREADS_INCLUDE_DIR ../include/pthreads)
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

set(DEF_FILE ../${PROJECT}${POSTFIX}.def)

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
    configure_file("${PROJECT_SOURCE_DIR}/${PROJECT}${POSTFIX}.def.in" "${PROJECT_SOURCE_DIR}/${DEF_FILE}")
endif()

include_directories(${INCLUDE_DIR})

set(EXPORT_HEADERS
    ${INCLUDE_DIR}/DXErrorCodes.h
    ${INCLUDE_DIR}/DXFeed.h
    ${INCLUDE_DIR}/DXTypes.h
    ${INCLUDE_DIR}/EventData.h
    ${INCLUDE_DIR}/RecordData.h
    )
 
set (HEADER_FILES 
    Candle.h
    ClientMessageProcessor.h
    ConnectionContextData.h
    DXAlgorithms.h
    DXErrorHandling.h
    DXMemory.h
    DXNetwork.h
    DXPMessageData.h
    DXSockets.h
    DXThreads.h
    EventManager.h
    EventSubscription.h
    Logger.h
    ObjectArray.h
    PriceLevelBook.h
    RegionalBook.h
    PrimitiveTypes.h
    resource.h
    RecordTranscoder.h
    ServerMessageProcessor.h
    Snapshot.h
    TaskQueue.h
    Version.h
    )
 
 set (PARSER_HEADERS
    BufferedInput.h
    BufferedIOCommon.h
    BufferedOutput.h
    ConfigurationDeserializer.h
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
    ConfigurationDeserializer.c
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
    Candle.c
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
    EventManager.c
    EventSubscription.c
    Logger.c
    ObjectArray.c
    PriceLevelBook.c
    RegionalBook.c
    RecordTranscoder.c
    ServerMessageProcessor.c
    Snapshot.c
    TaskQueue.c
    Win32.c
    ${DEF_FILE}) 
    
set (RESOURCE_FILES
    DXFeed.rc
    )

set (ALL_HEADERS
    ${HEADER_FILES}
    ${EXPORT_HEADERS}
    ${PARSER_HEADERS}
    )
    
set (ALL_SOURCES
    ${SOURCE_FILES}
    ${PARSER_SOURCES}
    ${PARSER_TESTS}
    ${RESOURCE_FILES}
    )
    
set (ADDITIONAL_LIBRARIES "")

if (MSVC)
    set(CMAKE_C_FLAGS_DEBUG "/TC /RTC1 /MTd /Od")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /MT /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
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


set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
install(TARGETS ${PROJECT}
    DESTINATION "bin/${TARGET_PLATFORM}"
)
install(DIRECTORY "${INCLUDE_DIR}" "../scripts" "../src"
    DESTINATION "."
    PATTERN "build" EXCLUDE
)
install(FILES "../DXFeed.def" "../DXFeedd.def" "../DXFeed_64.def" "../DXFeedd_64.def" 
    "../DXFeed.sln" "../DXFeed.vcxproj" "../DXFeed.vcxproj.filters" "../Makefile" 
    "../CMakeLists.txt" "../build.bat" "../make_package.bat" "../ReleaseNotes.txt"
    DESTINATION "."
)
if(MSVC)
    set(DEBUG_PDB_PATH ${EXECUTABLE_OUTPUT_PATH}/Debug)
    install(FILES ${DEBUG_PDB_PATH}/${PROJECT}d.pdb ${DEBUG_PDB_PATH}/${PROJECT}d_64.pdb
        DESTINATION "bin/${TARGET_PLATFORM}"
        CONFIGURATIONS Debug
        OPTIONAL
    )
    set(RELEASE_PDB_PATH ${EXECUTABLE_OUTPUT_PATH}/Release)
    install(FILES ${RELEASE_PDB_PATH}/${PROJECT}.pdb ${RELEASE_PDB_PATH}/${PROJECT}_64.pdb
        DESTINATION "bin/${TARGET_PLATFORM}"
        CONFIGURATIONS Release
        OPTIONAL
    )
endif(MSVC)

set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)
