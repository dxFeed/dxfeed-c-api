@echo off
rem Script CPack archives with specified platform and configuration. After
rem packing all files extracted to single release directory.
rem Usage:
rem     combine_package <project-name> <configuration> <platform> <version>
rem where:
rem     project-name    - The name of target project package
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

set PROJECT_NAME=%1
set CONFIG=%2
set PLATFORM=%3
set VERSION=%4

if [%PROJECT_NAME%] EQU [] (
    echo ERROR: Project package name is not specified!
    goto exit_error
)

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
rem if [%VERSION%] EQU [] (
rem     echo ERROR: Version is not specified!
rem     goto exit_error
rem )

cpack -G ZIP -C %CONFIG% --config %PLATFORM%\DXFeedAllCPackConfig.cmake
if %ERRORLEVEL% GEQ 1 goto exit_error
set WORK_DIR=_CPack_Packages
set PACKAGE_NAME=%PROJECT_NAME%-%VERSION%-%PLATFORM%
set ARCHIVE_NAME=%PROJECT_NAME%-%VERSION%-%PLATFORM%%CONFIG%
move /Y %PACKAGE_NAME%.zip %WORK_DIR%\%ARCHIVE_NAME%.zip
7z x -y -o%WORK_DIR% %WORK_DIR%\%ARCHIVE_NAME%.zip
if %ERRORLEVEL% GEQ 1 goto exit_error

:exit_success
exit /b 0
:exit_error
exit /b 1
:end