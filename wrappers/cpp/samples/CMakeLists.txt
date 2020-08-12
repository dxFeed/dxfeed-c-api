project(DXFeedCppWrapperSamples LANGUAGES CXX)

include_directories(../src)
add_subdirectory(ConnectionSample)

set(BUILD_FILES
    CMakeLists.txt
    )
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")

install(FILES ${BUILD_FILES}
    DESTINATION "wrappers/cpp/samples"
    CONFIGURATIONS Release
    )

set(CPACK_PACKAGE_VENDOR "dxFeed Solutions DE GmbH")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)