cmake_minimum_required(VERSION 3.0.0)

cmake_policy(SET CMP0015 NEW)

set(PROJECT UnitTests2)
project(${PROJECT} LANGUAGES CXX)

set(DXFEED_PARSER_SOURCES
        ../../src/BufferedInput.c
        ../../src/BufferedIOCommon.c
        ../../src/BufferedOutput.c
        ../../src/ConfigurationDeserializer.c
        ../../src/DataStructures.c
        ../../src/Decimal.c
        ../../src/RecordBuffers.c
        ../../src/RecordFieldSetters.c
        ../../src/SymbolCodec.c
        )

set(DXFEED_SOURCE_FILES
        ../../src/Candle.c
        ../../src/ClientMessageProcessor.c
        ../../src/ConnectionContextData.c
        ../../src/DXAddressParser.c
        ../../src/DXAlgorithms.c
        ../../src/DXErrorCodes.c
        ../../src/DXErrorHandling.c
        ../../src/DXFeed.c
        ../../src/DXMemory.c
        ../../src/DXNetwork.c
        ../../src/DXPMessageData.c
        ../../src/DXProperties.c
        ../../src/DXSockets.c
        ../../src/DXThreads.c
        ../../src/EventData.c
        ../../src/EventManager.c
        ../../src/EventSubscription.cpp
        ../../src/Logger.c
        ../../src/ObjectArray.c
        ../../src/PriceLevelBook.c
        ../../src/RegionalBook.c
        ../../src/RecordTranscoder.c
        ../../src/ServerMessageProcessor.c
        ../../src/Snapshot.c
        ../../src/TaskQueue.c
        ../../src/Version.c)

set(DXFEED_ALL_SOURCES
        ${SOURCE_FILES}
        ${PARSER_SOURCES}
        )

set(ADDITIONAL_LIBRARIES "")

if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_USRDLL -DDXFEED_EXPORTS)
    set(DXFEED_SOURCE_FILES
            ${DXFEED_SOURCE_FILES}
            ../../src/Win32.c
            )
    set(DXFEED_ALL_SOURCES
            ${DXFEED_ALL_SOURCES}
            ../../src/Win32.c
            )
    if (MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd /Zc:wchar_t /Zc:forScope /Gm- /W3 /Ob0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Gd /Zc:wchar_t /Zc:forScope /Gm- /W3 /Ob0")
        set(CMAKE_C_FLAGS_DEBUG "/RTC1 /MTd /Od")
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd /Od")
        set(CMAKE_C_FLAGS_RELEASE "/O2 /MT /Zi")
        set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MT /Zi")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
        set(ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES} ws2_32.lib)

        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES} dbghelp.lib)
        endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

        if (NOT DISABLE_TLS)
            link_directories(${LIB_TLS}/win/${TARGET_PLATFORM})
            set(ADDITIONAL_LIBRARIES
                    ${ADDITIONAL_LIBRARIES}
                    libtls-15${PLATFORM_POSTFIX}.lib
                    )
        endif (NOT DISABLE_TLS)

        # Hack for remove standard libraries from linking
        set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
        # End hack
    elseif (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")
        set(CMAKE_C_FLAGS_DEBUG "-g -O0")
        set(CMAKE_C_FLAGS_RELEASE "-O2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
        set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
        set(CMAKE_CXX_FLAGS_RELEASE "-O2")

        set(ADDITIONAL_LIBRARIES
                ${ADDITIONAL_LIBRARIES}
                ws2_32
                )
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(ADDITIONAL_LIBRARIES
                    ${ADDITIONAL_LIBRARIES}
                    dbghelp
                    )
        endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
        if (NOT DISABLE_TLS)
            link_directories(${LIB_TLS}/win/${TARGET_PLATFORM})
            set(ADDITIONAL_LIBRARIES
                    ${ADDITIONAL_LIBRARIES}
                    tls-15${PLATFORM_POSTFIX}
                    )
        endif (NOT DISABLE_TLS)
    else ()
        message("Unknown compiler")
    endif ()
elseif (MACOSX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fPIC")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")

    add_definitions(-DUSE_PTHREADS)
    set(DXFEED_SOURCE_FILES
            ${DXFEED_SOURCE_FILES}
            ../../src/Linux.c
            )
    set(DXFEED_ALL_SOURCES
            ${ALL_SOURCES}
            ../../src/Linux.c
            )
    set(ADDITIONAL_LIBRARIES
            ${ADDITIONAL_LIBRARIES}
            pthread
            )
    if (NOT DISABLE_TLS)
        link_directories(${LIB_TLS}/mac/${TARGET_PLATFORM})
        set(ADDITIONAL_LIBRARIES
                ${ADDITIONAL_LIBRARIES}
                tls ssl crypto
                )
    endif (NOT DISABLE_TLS)
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fPIC")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -fPIC")
    add_definitions(-DUSE_PTHREADS)

    set(DXFEED_SOURCE_FILES
            ${DXFEED_SOURCE_FILES}
            ../../src/Linux.c
            )
    set(DXFEED_ALL_SOURCES
            ${DXFEED_ALL_SOURCES}
            ../../src/Linux.c
            )
    set(ADDITIONAL_LIBRARIES
            ${ADDITIONAL_LIBRARIES}
            pthread
            )
    if (NOT DISABLE_TLS)
        link_directories(${LIB_TLS}/nix/${TARGET_PLATFORM})
        set(ADDITIONAL_LIBRARIES
                ${ADDITIONAL_LIBRARIES}
                tls ssl crypto
                )
    endif (NOT DISABLE_TLS)

    set(ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES} rt)
endif (WIN32)

if (NOT DISABLE_TLS)
    add_definitions(-DDXFEED_CODEC_TLS_ENABLED)
    include_directories(${LIB_TLS}/include)
endif (NOT DISABLE_TLS)

add_executable(
        ${PROJECT}
        src/AddressParserTests.cpp
        ${DXFEED_ALL_SOURCES}
)

add_dependencies(${PROJECT} DXFeed)

target_include_directories(
        ${PROJECT} PRIVATE
        ../../include
        ../../src
        thirdparty/catch2
)

set_target_properties(${PROJECT} PROPERTIES
        CMAKE_C_STANDARD 11
        CMAKE_C_STANDARD_REQUIRED on
        CMAKE_CXX_STANDARD 11
        CMAKE_CXX_STANDARD_REQUIRED on
        )

add_test(NAME "${PROJECT}"
        COMMAND ${PROJECT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )