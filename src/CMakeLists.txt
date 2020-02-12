cmake_minimum_required(VERSION 2.8.1)

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
set(LIB_TLS ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libressl)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
    add_definitions(-DMACOSX)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(POSTFIX "d")
endif ()

if (TARGET_PLATFORM STREQUAL "x64")
    set(PLATFORM_POSTFIX "_64")
    set(POSTFIX "${POSTFIX}${PLATFORM_POSTFIX}")
endif ()

set(DEF_FILE ../${PROJECT}${POSTFIX}.def)

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)
set(CMAKE_DEBUG_POSTFIX "d${PLATFORM_POSTFIX}")
set(CMAKE_RELEASE_POSTFIX "${PLATFORM_POSTFIX}")

project(${PROJECT})

# If library version is not specified through parameters version from the source will be used
if (NOT ${APP_VERSION} STREQUAL "")

    message(>>>>\ Application\ version\ original\ :\ ${APP_VERSION})
    string(REGEX REPLACE "([a-zA-Z]+)-" "" APP_VERSION ${APP_VERSION})
    string(REGEX REPLACE "-([a-zA-Z]+)$" "" APP_VERSION ${APP_VERSION})
    message(>>>>\ Application\ version\ processed:\ ${APP_VERSION})

    #separate version string on components
    string(REPLACE "." ";" VERSION_COMPONENTS_LIST ${APP_VERSION})
    list(LENGTH VERSION_COMPONENTS_LIST VERSION_COMPONENTS_COUNT)
    list(GET VERSION_COMPONENTS_LIST 0 C_API_VERSION_MAJOR)
    if (VERSION_COMPONENTS_COUNT GREATER 1)
        list(GET VERSION_COMPONENTS_LIST 1 C_API_VERSION_MINOR)
    endif ()
    if (VERSION_COMPONENTS_COUNT GREATER 2)
        list(GET VERSION_COMPONENTS_LIST 2 C_API_VERSION_PATCH)
    endif ()

    configure_file("${PROJECT_SOURCE_DIR}/Version.h.in" "${PROJECT_SOURCE_DIR}/Version.h")
    configure_file("${PROJECT_SOURCE_DIR}/${PROJECT}${POSTFIX}.def.in" "${PROJECT_SOURCE_DIR}/${DEF_FILE}")
endif ()

include_directories(${INCLUDE_DIR})

set(EXPORT_HEADERS
    ${INCLUDE_DIR}/DXErrorCodes.h
    ${INCLUDE_DIR}/DXFeed.h
    ${INCLUDE_DIR}/DXTypes.h
    ${INCLUDE_DIR}/EventData.h
    ${INCLUDE_DIR}/RecordData.h
    )

set(HEADER_FILES
    Candle.h
    ClientMessageProcessor.h
    ConnectionContextData.h
    DXAddressParser.h
    DXAlgorithms.h
    DXErrorHandling.h
    DXMemory.h
    DXNetwork.h
    DXPMessageData.h
    DXProperties.h
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

set(PARSER_HEADERS
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

set(PARSER_SOURCES
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

set(PARSER_TESTS
    TestParser.c
    )

set(SOURCE_FILES
    Candle.c
    ClientMessageProcessor.c
    ConnectionContextData.c
    DXAddressParser.c
    DXAlgorithms.c
    DXErrorCodes.c
    DXErrorHandling.c
    DXFeed.c
    DXMemory.c
    DXNetwork.c
    DXPMessageData.c
    DXProperties.c
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
    Version.c
    Win32.c
    ${DEF_FILE})

set(RESOURCE_FILES
    DXFeed.rc
    )

set(ALL_HEADERS
    ${HEADER_FILES}
    ${EXPORT_HEADERS}
    ${PARSER_HEADERS}
    )

set(ALL_SOURCES
    ${SOURCE_FILES}
    ${PARSER_SOURCES}
    ${PARSER_TESTS}
    ${RESOURCE_FILES}
    )

set(ADDITIONAL_LIBRARIES "")

if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_USRDLL -DDXFEED_EXPORTS)
    if (MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd /TC /Zc:wchar_t /Zc:forScope /Gm- /W3 /Ob0")
        set(CMAKE_C_FLAGS_DEBUG "/TC /RTC1 /MTd /Od")
        set(CMAKE_C_FLAGS_RELEASE "/O2 /MT /Zi")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
        set(ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES} ws2_32.lib)

        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES} dbghelp.lib)
        endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

        if (NOT DISABLE_TLS)
            link_directories(${LIB_TLS}/${TARGET_PLATFORM})
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
            link_directories(${LIB_TLS}/${TARGET_PLATFORM})
            set(ADDITIONAL_LIBRARIES
                ${ADDITIONAL_LIBRARIES}
                tls-15${PLATFORM_POSTFIX}
                )
        endif (NOT DISABLE_TLS)
    else ()
        message("Unknown compiler")
    endif ()
elseif(MACOSX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fPIC")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -fPIC")
    add_definitions(-DUSE_PTHREADS)
    set(SOURCE_FILES
            ${SOURCE_FILES}
            Linux.c
            )
    set(ALL_SOURCES
            ${ALL_SOURCES}
            Linux.c
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
#                tls
                )
    endif (NOT DISABLE_TLS)
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fPIC")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -fPIC")
    add_definitions(-DUSE_PTHREADS)

    set(SOURCE_FILES
        ${SOURCE_FILES}
        Linux.c
        )
    set(ALL_SOURCES
        ${ALL_SOURCES}
        Linux.c
        )
    set(ADDITIONAL_LIBRARIES
        ${ADDITIONAL_LIBRARIES}
        pthread
        )
    if (NOT DISABLE_TLS)
        link_directories(${LIB_TLS}/nix/${TARGET_PLATFORM})
        set(ADDITIONAL_LIBRARIES
            ${ADDITIONAL_LIBRARIES}
            tls
            )
    endif (NOT DISABLE_TLS)

    set(ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES} rt)
endif (WIN32)

if (NOT DISABLE_TLS)
    add_definitions(-DDXFEED_CODEC_TLS_ENABLED)
    include_directories(${LIB_TLS}/include)
endif (NOT DISABLE_TLS)

source_group("Export" FILES ${EXPORT_HEADERS})
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Parser\\Headers" FILES ${PARSER_HEADERS})
source_group("Parser\\Sources" FILES ${PARSER_SOURCES})
source_group("Parser\\Test" FILES ${PARSER_TESTS})
source_group("Source Files" FILES ${SOURCE_FILES})

add_library(${PROJECT} SHARED ${ALL_HEADERS} ${ALL_SOURCES})

target_link_libraries(${PROJECT} PRIVATE ${ADDITIONAL_LIBRARIES})

if (WIN32)
    set_target_properties(${PROJECT} PROPERTIES PREFIX "")
elseif(MACOSX)
    set_target_properties(
            ${PROJECT}
            PROPERTIES
            INSTALL_RPATH "@executable_path/."
    )
endif ()

if (NOT DISABLE_TLS)
    if (WIN32)
        ADD_CUSTOM_COMMAND(
            TARGET ${PROJECT}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/${TARGET_PLATFORM}/libtls-15${PLATFORM_POSTFIX}.dll ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/${TARGET_PLATFORM}/libssl-43${PLATFORM_POSTFIX}.dll ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/${TARGET_PLATFORM}/libcrypto-41${PLATFORM_POSTFIX}.dll ${CMAKE_BINARY_DIR}/$(Configuration)
        )
    elseif(MACOSX)
        ADD_CUSTOM_COMMAND(
                TARGET ${PROJECT}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.46.0.0.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.46.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.a ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.48.0.0.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.48.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.a ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.20.0.0.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.20.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.a ${CMAKE_BINARY_DIR}/$(Configuration)
                COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.dylib ${CMAKE_BINARY_DIR}/$(Configuration)
        )
    else ()
        ADD_CUSTOM_COMMAND(
            TARGET ${PROJECT}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libcrypto.so.41.1.0 ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libcrypto.so.41 ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libcrypto.so ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libssl.so.43.2.0 ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libssl.so.43 ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libssl.so ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libtls.so.15.4.0 ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libtls.so.15 ${CMAKE_BINARY_DIR}/$(Configuration)
            COMMAND ${CMAKE_COMMAND} -E copy ${LIB_TLS}/nix/${TARGET_PLATFORM}/libtls.so ${CMAKE_BINARY_DIR}/$(Configuration)
        )
    endif (WIN32)
endif (NOT DISABLE_TLS)


set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
install(TARGETS ${PROJECT}
    DESTINATION "bin/${TARGET_PLATFORM}"
    )
install(DIRECTORY "${INCLUDE_DIR}" "../scripts" "../src"
    DESTINATION "."
    PATTERN "build" EXCLUDE
    )
install(FILES "../DXFeed.def" "../DXFeedd.def" "../DXFeed_64.def" "../DXFeedd_64.def"
    "../DXFeed.sln" "../DXFeed.vcxproj" "../DXFeed.vcxproj.filters"
    "../CMakeLists.txt" "../build.bat" "../build.sh" "../make_package.bat" "../make_package.sh" "../ReleaseNotes.txt"
    DESTINATION "."
    )

if (NOT DISABLE_TLS)
    if (WIN32)
        install(FILES
            "${LIB_TLS}/${TARGET_PLATFORM}/libtls-15${PLATFORM_POSTFIX}.dll"
            "${LIB_TLS}/${TARGET_PLATFORM}/libssl-43${PLATFORM_POSTFIX}.dll"
            "${LIB_TLS}/${TARGET_PLATFORM}/libcrypto-41${PLATFORM_POSTFIX}.dll"
            DESTINATION "bin/${TARGET_PLATFORM}"
            )
    elseif (MACOSX)
        install(FILES
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.46.0.0.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.46.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.a"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libcrypto.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.48.0.0.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.48.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.a"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libssl.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.20.0.0.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.20.dylib"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.a"
                "${LIB_TLS}/mac/${TARGET_PLATFORM}/libtls.dylib"
                DESTINATION "bin/${TARGET_PLATFORM}"
                )
    else ()
        install(FILES
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libtls.so.15.4.0"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libtls.so.15"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libtls.so"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libssl.so.43.2.0"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libssl.so.43"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libssl.so"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libcrypto.so.41.1.0"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libcrypto.so.41"
            "${LIB_TLS}/nix/${TARGET_PLATFORM}/libcrypto.so"
            DESTINATION "bin/${TARGET_PLATFORM}"
            )
    endif (WIN32)
    install(FILES "../COPYING.libressl"
        DESTINATION "."
        )
    install(DIRECTORY "../lib/libressl"
        DESTINATION "lib"
        PATTERN "build" EXCLUDE
        )
    set(PACKAGE_SUFFIX "")
else (NOT DISABLE_TLS)
    set(PACKAGE_SUFFIX "-no-tls")
endif (NOT DISABLE_TLS)

if (MSVC)
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
endif (MSVC)

set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}${PACKAGE_SUFFIX}")
include(CPack)
