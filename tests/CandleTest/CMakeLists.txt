cmake_minimum_required (VERSION 2.6)

cmake_policy(SET CMP0015 NEW)
 
set(PROJECT CandleTest) 
set(INCLUDE_DIR ../../include)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(POSTFIX "d")
else()
    set(POSTFIX "")
endif()
set(LIB_DXFEED_SRC_DIR ../../src)
set(LIB_DXFEED_PROJ DXFeed)
set(LIB_DXFEED_NAME ${LIB_DXFEED_PROJ}${POSTFIX})
set(LIB_DXFEED_OUT_DIR ${CMAKE_BINARY_DIR}/${LIB_DXFEED_PROJ})

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)
 
project(${PROJECT}) 

include_directories(${INCLUDE_DIR})

add_subdirectory(${LIB_DXFEED_SRC_DIR} ${LIB_DXFEED_OUT_DIR})

link_directories(${LIB_DXFEED_OUT_DIR})
 
set(SOURCE_FILES
    CandleTest.c
    )
    
set(ADDITIONAL_PROPERTIES "")

if(MSVC)
    set(CMAKE_C_FLAGS_DEBUG "/TC /RTC1 /MDd /Od -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/Ox /MD -DNDEBUG -DWIN32")
    add_definitions(-D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE /Gd /TC /Zc:wchar_t /Zc:forScope /Gm- /W3 /Ob0 /Zi)
    
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

target_link_libraries(${PROJECT} ${LIB_DXFEED_NAME})

set_target_properties(${PROJECT} 
    PROPERTIES 
    DEBUG_POSTFIX "d"
    LINK_FLAGS "${ADDITIONAL_PROPERTIES}"
)

set_target_properties(${LIB_DXFEED_PROJ} 
    PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
)

add_dependencies(${PROJECT} ${LIB_DXFEED_PROJ})
