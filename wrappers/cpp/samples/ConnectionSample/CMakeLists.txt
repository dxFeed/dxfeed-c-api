project(DXFeedCppWrapperConnectionSample LANGUAGES CXX)

link_directories(${LIB_DXFEED_OUT_DIR})

add_executable(DXFeedCppWrapperConnectionSample
    main.cpp
    $<TARGET_OBJECTS:DXFeedCppWrapper>)

add_dependencies(DXFeedCppWrapperConnectionSample ${LIB_DXFEED_PROJ})

target_link_libraries(DXFeedCppWrapperConnectionSample DXFeed)

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

install(TARGETS DXFeedCppWrapperConnectionSample
    DESTINATION "bin/${TARGET_PLATFORM}"
    CONFIGURATIONS Release
    )
install(FILES ${SOURCE_FILES} ${PROJECT_FILES} ${BUILD_FILES}
    DESTINATION "wrappers/cpp/samples/ConnectionSample"
    CONFIGURATIONS Release
    )

set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)