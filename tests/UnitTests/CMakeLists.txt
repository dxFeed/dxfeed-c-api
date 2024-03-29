cmake_minimum_required(VERSION 3.10.0)

cmake_policy(SET CMP0015 NEW)

set(PROJECT UnitTests) 
set(INCLUDE_DIR ../../include)
set(TARGET_PLATFORM "x86" CACHE STRING "Target platform specification")
set(PLATFORM_POSTFIX "")
set(LIB_TLS ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libressl)
if(TARGET_PLATFORM STREQUAL "x64")
    set(PLATFORM_POSTFIX "_64")
endif()
set(DEBUG_POSTFIX "d${PLATFORM_POSTFIX}")
set(RELEASE_POSTFIX ${PLATFORM_POSTFIX})
set(LIB_DXFEED_SRC_DIR ../../src)
set(LIB_DXFEED_PROJ DXFeed)
set(LIB_DXFEED_NAME ${LIB_DXFEED_PROJ})
set(LIB_DXFEED_OUT_DIR ${CMAKE_BINARY_DIR}/${LIB_DXFEED_PROJ})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE INTERNAL "" FORCE)

project(${PROJECT}) 

include_directories(
    ${INCLUDE_DIR}
    ../../src
    )

if (NOT TARGET ${LIB_DXFEED_PROJ})
    add_subdirectory(${LIB_DXFEED_SRC_DIR} ${LIB_DXFEED_OUT_DIR})
endif()

link_directories(${LIB_DXFEED_OUT_DIR})

set(COMMON_HEADERS
    ${LIB_DXFEED_SRC_DIR}/Candle.h
    ${LIB_DXFEED_SRC_DIR}/ClientMessageProcessor.h
    ${LIB_DXFEED_SRC_DIR}/DXAddressParser.h
    ${LIB_DXFEED_SRC_DIR}/DXAlgorithms.h
    ${INCLUDE_DIR}/DXErrorCodes.h
    ${LIB_DXFEED_SRC_DIR}/DXErrorHandling.h
    ${LIB_DXFEED_SRC_DIR}/DXMemory.h
    ${LIB_DXFEED_SRC_DIR}/DXPMessageData.h
    ${LIB_DXFEED_SRC_DIR}/DXProperties.h
    ${LIB_DXFEED_SRC_DIR}/DXThreads.h
    ${INCLUDE_DIR}/EventData.h
    ${LIB_DXFEED_SRC_DIR}/EventManager.h
    ${LIB_DXFEED_SRC_DIR}/ObjectArray.h
    ${LIB_DXFEED_SRC_DIR}/PriceLevelBook.h
    ${LIB_DXFEED_SRC_DIR}/RegionalBook.h
    ${LIB_DXFEED_SRC_DIR}/PrimitiveTypes.h
    ${LIB_DXFEED_SRC_DIR}/Snapshot.h
    ${LIB_DXFEED_SRC_DIR}/TaskQueue.h
    ${LIB_DXFEED_SRC_DIR}/WideDecimal.h
    ${LIB_DXFEED_SRC_DIR}/WideDecimal.hpp
    )
    
set(COMMON_SOURCES
    ${LIB_DXFEED_SRC_DIR}/Candle.c
    ${LIB_DXFEED_SRC_DIR}/ConnectionContextData.c
    ${LIB_DXFEED_SRC_DIR}/DXAddressParser.c
    ${LIB_DXFEED_SRC_DIR}/DXAlgorithms.c
    ${LIB_DXFEED_SRC_DIR}/DXErrorCodes.c
    ${LIB_DXFEED_SRC_DIR}/DXErrorHandling.c
    ${LIB_DXFEED_SRC_DIR}/DXMemory.c
    ${LIB_DXFEED_SRC_DIR}/DXNetwork.c
    ${LIB_DXFEED_SRC_DIR}/DXPMessageData.c
    ${LIB_DXFEED_SRC_DIR}/DXProperties.c
    ${LIB_DXFEED_SRC_DIR}/DXSockets.c
    ${LIB_DXFEED_SRC_DIR}/DXThreads.c
    ${LIB_DXFEED_SRC_DIR}/EventData.c
    ${LIB_DXFEED_SRC_DIR}/EventManager.c
    ${LIB_DXFEED_SRC_DIR}/ObjectArray.c
    ${LIB_DXFEED_SRC_DIR}/PriceLevelBook.c
    ${LIB_DXFEED_SRC_DIR}/RegionalBook.c
    ${LIB_DXFEED_SRC_DIR}/Snapshot.c
    ${LIB_DXFEED_SRC_DIR}/TaskQueue.c
    ${LIB_DXFEED_SRC_DIR}/Win32.c
    ${LIB_DXFEED_SRC_DIR}/WideDecimal.cpp
    )
    
set(EVENT_SUBSCRIPTION_SOURCES
    ${LIB_DXFEED_SRC_DIR}/EventSubscription.h
    ${LIB_DXFEED_SRC_DIR}/EventSubscription.hpp
    ${LIB_DXFEED_SRC_DIR}/EventSubscription.cpp
    )
    
set(HEADER_FILES
    AddressParserTest.h
    AlgorithmsTest.h
    CandleTest.h
    ConnectionTest.h
    DXNetworkTests.h
    EventDynamicSubscriptionTest.h
    EventSubscriptionTest.h
    OrderSourceConfigurationTest.h
    SnapshotTests.h
    TestHelper.h
    )
    
set(PARSER_HEADERS
    ${LIB_DXFEED_SRC_DIR}/ConfigurationDeserializer.h
    ${LIB_DXFEED_SRC_DIR}/DataStructures.h
    ${LIB_DXFEED_SRC_DIR}/Logger.h
    ${LIB_DXFEED_SRC_DIR}/SymbolCodec.h
    )
    
set(PARSER_SOURCES
    ${LIB_DXFEED_SRC_DIR}/BufferedInput.c
    ${LIB_DXFEED_SRC_DIR}/BufferedIOCommon.c
    ${LIB_DXFEED_SRC_DIR}/BufferedOutput.c
    ${LIB_DXFEED_SRC_DIR}/ClientMessageProcessor.c
    ${LIB_DXFEED_SRC_DIR}/ConfigurationDeserializer.c
    ${LIB_DXFEED_SRC_DIR}/DataStructures.c
    ${LIB_DXFEED_SRC_DIR}/Decimal.c
    ${LIB_DXFEED_SRC_DIR}/Logger.c
    ${LIB_DXFEED_SRC_DIR}/RecordBuffers.c
    ${LIB_DXFEED_SRC_DIR}/RecordFieldSetters.c
    ${LIB_DXFEED_SRC_DIR}/RecordTranscoder.c
    ${LIB_DXFEED_SRC_DIR}/ServerMessageProcessor.c
    ${LIB_DXFEED_SRC_DIR}/SymbolCodec.c
    )
 
set(SOURCE_FILES
    AddressParserTest.c
    AlgorithmsTest.c
    CandleTest.c
    ConnectionTest.c
    DXNetworkTests.c
    EventDynamicSubscriptionTest.cpp
    EventSubscriptionTest.c
    OrderSourceConfigurationTest.c
    SnapshotTests.c
    SnapshotUnitTests.c
    TestHelper.c
    UnitTests.c
    )
    
set(ALL_SOURCES
    ${COMMON_HEADERS}
    ${COMMON_SOURCES}
    ${EVENT_SUBSCRIPTION_SOURCES}
    ${HEADER_FILES}
    ${PARSER_HEADERS}
    ${PARSER_SOURCES}
    ${SOURCE_FILES}
    )
    
set(ADDITIONAL_LIBRARIES "")
set(ADDITIONAL_PROPERTIES "")

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd /TC /Zc:wchar_t /Zc:forScope /Gm- /W3 /Ob0 /Zi")
    set(CMAKE_C_FLAGS_DEBUG "/TC /RTC1 /MDd /Od -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/Ox /MD -DNDEBUG -DWIN32")
    add_definitions(-D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)

    set (ADDITIONAL_LIBRARIES
        ${ADDITIONAL_LIBRARIES}
        ws2_32.lib
    )
    if (NOT DISABLE_TLS)
        link_directories(${LIB_TLS}/${TARGET_PLATFORM})
        set (ADDITIONAL_LIBRARIES
            ${ADDITIONAL_LIBRARIES}
            libtls-15${PLATFORM_POSTFIX}.lib
        )
    endif (NOT DISABLE_TLS)
    
    set(ADDITIONAL_PROPERTIES
        ${ADDITIONAL_PROPERTIES}
        /SUBSYSTEM:CONSOLE
    )
    
    # Hack for remove standard libraries from linking
    set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
    # End hack
    
elseif(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-O2 -DUSE_PTHREADS -std=gnu99)
else()
    message("Unknown compiler")
endif()

if (NOT DISABLE_TLS)
    add_definitions(-DDXFEED_CODEC_TLS_ENABLED)
    include_directories(${LIB_TLS}/include)
endif (NOT DISABLE_TLS)

source_group("Common\\Headers" FILES ${COMMON_HEADERS})
source_group("Common\\Sources" FILES ${COMMON_SOURCES})
source_group("Event Subscription" FILES ${EVENT_SUBSCRIPTION_SOURCES})
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Parser\\Headers" FILES ${PARSER_HEADERS})
source_group("Parser\\Sources" FILES ${PARSER_SOURCES})
source_group("Source Files" FILES ${SOURCE_FILES})

add_executable (${PROJECT} ${ALL_SOURCES})

target_link_libraries(${PROJECT} 
    ${ADDITIONAL_LIBRARIES}
    debug ${LIB_DXFEED_NAME}${DEBUG_POSTFIX} 
    optimized ${LIB_DXFEED_NAME}${RELEASE_POSTFIX}
    )

set_target_properties(${PROJECT} 
    PROPERTIES 
    DEBUG_POSTFIX "${DEBUG_POSTFIX}"
    RELEASE_POSTFIX "${RELEASE_POSTFIX}"
    LINK_FLAGS "${ADDITIONAL_PROPERTIES}"
)

add_dependencies(${PROJECT} ${LIB_DXFEED_PROJ})

if (MSVC)
    if (NOT DISABLE_TLS)
        ADD_CUSTOM_COMMAND(
            TARGET ${PROJECT}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/${TARGET_PLATFORM}/libtls-15${PLATFORM_POSTFIX}.dll ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/${TARGET_PLATFORM}/libssl-43${PLATFORM_POSTFIX}.dll ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/${TARGET_PLATFORM}/libcrypto-41${PLATFORM_POSTFIX}.dll ${CMAKE_BINARY_DIR}
        )
    endif (NOT DISABLE_TLS)
endif (MSVC)


set(PROJECT_FILES
    ${PROJECT}.vcxproj
    ${PROJECT}.vcxproj.filters
)
set(BUILD_FILES
    CMakeLists.txt
)
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
install(TARGETS ${PROJECT}
    DESTINATION "bin/${TARGET_PLATFORM}"
    CONFIGURATIONS Release
)
install(FILES ${HEADER_FILES} ${SOURCE_FILES} ${PROJECT_FILES} ${BUILD_FILES}
    DESTINATION "tests/${PROJECT}"
    CONFIGURATIONS Release
)
set(CPACK_PACKAGE_VENDOR "Devexperts LLC")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)
