set(PROJECT DXFeedCppWrapperConnectionSample)
project(${PROJECT} LANGUAGES CXX)

link_directories(${LIB_DXFEED_OUT_DIR})

set(ADDITIONAL_LIBRARIES "")

if (NOT WIN32)
    if (NOT DISABLE_TLS)
        link_directories(${LIB_TLS}/nix/${TARGET_PLATFORM})
        set(ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES} tls ssl crypto)
    endif (NOT DISABLE_TLS)
endif (NOT WIN32)

add_executable(${PROJECT}
    main.cpp
    $<TARGET_OBJECTS:DXFeedCppWrapper>)

add_dependencies(${PROJECT} ${LIB_DXFEED_PROJ})

target_link_libraries(${PROJECT} DXFeed ${ADDITIONAL_LIBRARIES})

set_target_properties(${PROJECT}
    PROPERTIES
    DEBUG_POSTFIX "${DEBUG_POSTFIX}"
    RELEASE_POSTFIX "${RELEASE_POSTFIX}"
)

set(PROJECT_FILES
    DXFeedCppWrapperConnectionSample.vcxproj
    DXFeedCppWrapperConnectionSample.vcxproj.filters
    )

set(SOURCE_FILES
    main.cpp
    )

set(BUILD_FILES
    CMakeLists.txt
    )
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")

install(TARGETS ${PROJECT}
    DESTINATION "bin/${TARGET_PLATFORM}"
    )
install(FILES ${SOURCE_FILES} ${PROJECT_FILES} ${BUILD_FILES}
    DESTINATION "wrappers/cpp/samples/ConnectionSample"
    CONFIGURATIONS Release
    )

set(CPACK_PACKAGE_VENDOR "Devexperts LLC")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)