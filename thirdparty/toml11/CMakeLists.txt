cmake_minimum_required(VERSION 3.5)
enable_testing()

project(toml11 VERSION 3.6.0)

option(toml11_BUILD_TEST "Build toml tests" OFF)
option(toml11_TEST_WITH_ASAN  "use LLVM address sanitizer" OFF)
option(toml11_TEST_WITH_UBSAN "use LLVM undefined behavior sanitizer" OFF)

include(CheckCXXCompilerFlag)
set(CMAKE_CXX_STANDARD 11 CACHE STRING "The C++ standard whose features are requested to build all targets.")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Boolean describing whether the value of CXX_STANDARD is a requirement.")
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Boolean specifying whether compiler specific extensions are requested.")

if(MSVC)
    add_definitions("/Zc:__cplusplus") # define __cplusplus value correctly
    add_definitions("/utf-8")          # enable to use u8"" literal
endif()

# Set some common directories
include(GNUInstallDirs)
set(toml11_install_cmake_dir         ${CMAKE_INSTALL_LIBDIR}/cmake/toml11)
set(toml11_install_include_dir       ${CMAKE_INSTALL_INCLUDEDIR})
set(toml11_config_dir                ${CMAKE_CURRENT_BINARY_DIR}/cmake/)
set(toml11_config                    ${toml11_config_dir}/toml11Config.cmake)
set(toml11_config_version            ${toml11_config_dir}/toml11ConfigVersion.cmake)

add_library(toml11 INTERFACE)
target_include_directories(toml11 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${toml11_install_include_dir}>
)
add_library(toml11::toml11 ALIAS toml11)

# Write config and version config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${toml11_config_version}
    VERSION ${toml11_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    cmake/toml11Config.cmake.in
    ${toml11_config}
    INSTALL_DESTINATION ${toml11_install_cmake_dir}
    PATH_VARS toml11_install_cmake_dir
)
