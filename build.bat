@echo off
rem This script runs building project using CMakeLists.txt from current folder.
rem The result of building places into BUILD_DIR directory.
rem Usage: 
rem     build.bat [<configuration>] [<platform>] [rebuild|clean] [no-tls] [<CMake Generator>]
rem Where
rem     <configuration>   - Debug or Release
rem     <platform>        - x86 or x64
rem     clean             - removes build directory
rem     rebuild           - performs clean and build
rem     no-tls            - performs build without TLS\SSL support
rem     <CMake Generator>:
rem         vs2015        - performs build using "Visual Studio 14 2015" CMake generator
rem         vs2017        - performs build using "Visual Studio 15 2017" CMake generator
rem         vs2019        - performs build using "Visual Studio 16 2019" CMake generator
rem         vs2022        - performs build using "Visual Studio 17 2022" CMake generator
rem The default configuration is 'Release x86 vs2022'.
rem
rem WARNING: you can set the next environment variables
rem     APP_VERSION - the version of target in format major[.minor[.patch]], i.e. 1.2.6
rem                   If you want to specify version you must set 'set APP_VERSION=...'
rem                   in your caller script or command line. Otherwise the default version 
rem                   from sources will be used.

setlocal

set BUILD_DIR=%~dp0\build
set CONFIG=
set PLATFORM=
set CMAKE_PLATFORM=Win32
set DISABLE_TLS=OFF
set GENERATOR=Visual Studio 17 2022

for %%A in (%*) do (
    if [%%A] EQU [Debug] (
        set CONFIG=%%A
    ) else if [%%A] EQU [Release] (
        set CONFIG=%%A
    ) else if [%%A] EQU [x86] (
        set PLATFORM=%%A
    ) else if [%%A] EQU [x64] (
        set PLATFORM=%%A
        set CMAKE_PLATFORM=x64
    ) else if [%%A] EQU [clean] (
        if exist %BUILD_DIR% (
            RMDIR /S /Q %BUILD_DIR%
            if %ERRORLEVEL% GEQ 1 goto exit_error
        )
        goto exit_success
    ) else if [%%A] EQU [rebuild] (    
        if exist %BUILD_DIR% (
            RMDIR /S /Q %BUILD_DIR%
            if %ERRORLEVEL% GEQ 1 goto exit_error
        )
    ) else if [%%A] EQU [no-tls] (
       set DISABLE_TLS=ON
    ) else if [%%A] EQU [vs2015] (
       set GENERATOR=Visual Studio 14 2015
    ) else if [%%A] EQU [vs2017] (
       set GENERATOR=Visual Studio 15 2017
    ) else if [%%A] EQU [vs2019] (
       set GENERATOR=Visual Studio 16 2019
    ) else if [%%A] EQU [vs2022] (
       set GENERATOR=Visual Studio 17 2022
    ) else (
        goto usage
    )
)

:build
if [%CONFIG%] EQU [] set CONFIG=Release
if [%PLATFORM%] EQU [] set PLATFORM=x86
set BUILD_DIR=%BUILD_DIR%\%PLATFORM%

if not exist %BUILD_DIR% (mkdir %BUILD_DIR%)
cd %BUILD_DIR%

echo Start building %CONFIG% %PLATFORM%...
cmake -DCMAKE_BUILD_TYPE=%CONFIG% -G "%GENERATOR%" -A %CMAKE_PLATFORM% -DDISABLE_TLS=%DISABLE_TLS% -DTARGET_PLATFORM=%PLATFORM% -DAPP_VERSION=%APP_VERSION% ..\..
if %ERRORLEVEL% GEQ 1 goto build_error

cmake --build . --config %CONFIG% -- -maxcpucount:8
if %ERRORLEVEL% GEQ 1 goto build_error

:build_success
cd..\..
echo Build success.
goto exit_success

:build_error
cd..\..
echo Build error!
goto exit_error

:usage
echo Usage: %0 [^<configuration^>] [^<platform^>] [rebuild^|clean] [no-tls] [^<CMake Generator^>]
echo        ^<configuration^>      - Debug or Release
echo        ^<platform^>           - x86 or x64
echo        clean                - removes build directory
echo        rebuild              - performs clean and build
echo        no-tls               - performs build without TLS\SSL support
echo        ^<CMake Generator^>:
echo            vs2015           - performs build using "Visual Studio 14 2015" CMake generator
echo            vs2017           - performs build using "Visual Studio 15 2017" CMake generator
echo            vs2019           - performs build using "Visual Studio 16 2019" CMake generator
echo            vs2022           - performs build using "Visual Studio 17 2022" CMake generator
goto exit_error

:exit_success
exit /b 0
:exit_error
exit /b 1