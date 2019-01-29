project(DXFeedCppWrapper LANGUAGES CXX)

set(DX_FEED_CPP_WRAPPER_SOURCES Error.hpp Connection.hpp Connection.cpp Subscription.cpp Subscription.hpp StringConverter.hpp Event.cpp Event.hpp Utils.hpp)

add_library(DXFeedCppWrapper OBJECT ${DX_FEED_CPP_WRAPPER_SOURCES})

set(SOURCE_FILES
    ${DX_FEED_CPP_WRAPPER_SOURCES}
    )

set(BUILD_FILES
    CMakeLists.txt
    )
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")

install(FILES ${SOURCE_FILES} ${PROJECT_FILES} ${BUILD_FILES}
    DESTINATION "wrappers/cpp/src"
    CONFIGURATIONS Release
    )

set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)