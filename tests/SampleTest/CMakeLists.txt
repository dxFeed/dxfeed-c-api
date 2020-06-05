cmake_minimum_required (VERSION 2.8.1)

cmake_policy(SET CMP0015 NEW)
 
set(PROJECT SampleTest) 
set(INCLUDE_DIR 
    ../../include
    ../../src
    )
set(TARGET_PLATFORM "x86" CACHE STRING "Target platform specification")
set(PLATFORM_POSTFIX "")
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

include_directories(${INCLUDE_DIR})

if (NOT TARGET ${LIB_DXFEED_PROJ})
    add_subdirectory(${LIB_DXFEED_SRC_DIR} ${LIB_DXFEED_OUT_DIR})
endif()

link_directories(${LIB_DXFEED_OUT_DIR})
 
set(SOURCE_FILES
    SampleTest.c
    )
    
set(ADDITIONAL_PROPERTIES "")

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd /TC /Zc:wchar_t /Zc:forScope /Gm- /W3 /Ob0 /Zi")
    set(CMAKE_C_FLAGS_DEBUG "/TC /RTC1 /MDd /Od -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/Ox /MD -DNDEBUG -DWIN32")
    add_definitions(-D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)

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

source_group("Source Files" FILES ${SOURCE_FILES}) 

add_executable (${PROJECT} ${SOURCE_FILES})

target_link_libraries(${PROJECT} debug ${LIB_DXFEED_NAME}${DEBUG_POSTFIX} optimized ${LIB_DXFEED_NAME}${RELEASE_POSTFIX})

set_target_properties(${PROJECT} 
    PROPERTIES 
    DEBUG_POSTFIX "${DEBUG_POSTFIX}"
    RELEASE_POSTFIX "${RELEASE_POSTFIX}"
    LINK_FLAGS "${ADDITIONAL_PROPERTIES}"
)

add_dependencies(${PROJECT} ${LIB_DXFEED_PROJ})

set(PROJECT_FILES
    ${PROJECT}.vcxproj
    ${PROJECT}.vcxproj.filters
)
set(BUILD_FILES
    build.bat
    CMakeLists.txt
)
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/DXFeedAllCPackConfig.cmake")
install(TARGETS ${PROJECT}
    DESTINATION "bin/${TARGET_PLATFORM}"
    CONFIGURATIONS Release
)
install(FILES ${SOURCE_FILES} ${PROJECT_FILES} ${BUILD_FILES}
    DESTINATION "tests/${PROJECT}"
    CONFIGURATIONS Release
)
set(CPACK_PACKAGE_VENDOR "Devexperts")
set(CPACK_PACKAGE_NAME "${PROJECT}")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT}-${APP_VERSION}-${TARGET_PLATFORM}")
include(CPack)
