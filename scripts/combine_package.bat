@echo off
rem Script CPack archives with specified platform and configuration. After
rem packing all files extracted to single release directory.
rem Usage:
rem     combine_package <configuration> <platform> <version>
rem where:
rem     configuration   - Debug or Release
rem     platform        - x86 or x64
rem     version         - version of application i.e. 1.2.6

setlocal

rem Check cpack application in PATH
where /q cpack
if %ERRORLEVEL% GEQ 1 (
    echo The 'cpack' application is missing. Ensure it is installed and placed in your PATH.
    goto exit_error
)
rem Check 7z archiver in PATH
where /q 7z
if %ERRORLEVEL% GEQ 1 (
    echo The '7z' application is missing. Ensure it is installed and placed in your PATH.
    goto exit_error
)

set CONFIG=%1
set PLATFORM=%2
set VERSION=%3

if [%CONFIG%] NEQ [Debug] (
    if [%CONFIG%] NEQ [Release] (
        echo ERROR: Invalid configuration value '%CONFIG%'
        goto exit_error
    )
)
if [%PLATFORM%] NEQ [x86] (
    if [%PLATFORM%] NEQ [x64] (
        echo ERROR: Invalid platform value '%PLATFORM%'
        goto exit_error
    )
)
if [%VERSION%] EQU [] (
    echo ERROR: Version is not specified!
    goto exit_error
)

cpack -G ZIP -C %CONFIG% --config %PLATFORM%\DXFeedAllCPackConfig.cmake
if %ERRORLEVEL% GEQ 1 goto exit_error
set PACKAGE_NAME=DXFeedAll-%VERSION%-%PLATFORM%
set ARCHIVE_NAME=DXFeedAll-%VERSION%-%PLATFORM%%CONFIG%
move /Y %PACKAGE_NAME%.zip %ARCHIVE_NAME%.zip
7z x -y %ARCHIVE_NAME%.zip
if %ERRORLEVEL% GEQ 1 goto exit_error

:exit_success
exit /b 0
:exit_error
exit /b 1
:end