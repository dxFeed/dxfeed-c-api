@echo off
rem Script CPack archives with specified platform and configuration. After
rem packing all files extracted to single release directory.
rem Usage:
rem     combine_package <project-name> <configuration> <platform> <version> [no-tls]
rem where:
rem     project-name    - The name of target project package
rem     configuration   - Debug or Release
rem     platform        - x86 or x64
rem     version         - version of application i.e. 1.2.6
rem
rem WARNING: you must set the next environment variables
rem     PACKAGE_WORK_DIR - the working directory where cpack result arhive will be stored and unpacked

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

rem Check PACKAGE_WORK_DIR environment variable
if [%PACKAGE_WORK_DIR%] EQU [] (
    echo ERROR: Environment variable PACKAGE_WORK_DIR is not specified!
    echo Please set 'set PACKAGE_WORK_DIR=...' in your caller script or command line.
    goto exit_error
)

set PROJECT_NAME=%1
set CONFIG=%2
set PLATFORM=%3
set VERSION=%4
set NO_TLS=%5
set PACKAGE_SUFFIX=

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

if [%NO_TLS%] EQU [no-tls] (
    set PACKAGE_SUFFIX=-no-tls
)

echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "PLATFORM: %PLATFORM%"
echo "CONFIG  : %CONFIG%"
echo "PROJECT : %PROJECT_NAME%"
echo "NO_TLS  : %NO_TLS%"
echo "VERSION : %VERSION%"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
more "%PLATFORM%\DXFeedAllCPackConfig.cmake"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

cpack -G ZIP -C %CONFIG% --config %PLATFORM%\DXFeedAllCPackConfig.cmake
if %ERRORLEVEL% GEQ 1 goto exit_error
set PACKAGE_NAME=%PROJECT_NAME%-%VERSION%-%PLATFORM%%PACKAGE_SUFFIX%
set ARCHIVE_NAME=%PROJECT_NAME%-%VERSION%-%PLATFORM%%CONFIG%%PACKAGE_SUFFIX%
move /Y %PACKAGE_NAME%.zip %PACKAGE_WORK_DIR%\%ARCHIVE_NAME%.zip
7z x -y -o%PACKAGE_WORK_DIR% %PACKAGE_WORK_DIR%\%ARCHIVE_NAME%.zip
if %ERRORLEVEL% GEQ 1 goto exit_error

:exit_success
exit /b 0
:exit_error
exit /b 1
:end