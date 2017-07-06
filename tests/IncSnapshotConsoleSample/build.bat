@echo off
rem This script runs building project using CMakeLists.txt from current folder.
rem The result of building places into BUILD_DIR directory.
rem Usage: 
rem     build.bat [<configuration>] [<platform>] [rebuild|clean]
rem Where
rem     <configuration> - Debug or Release
rem     <platform>      - x86 or x64
rem     clean           - removes build directory
rem     rebuild         - performs clean and build
rem The default configuration is 'Release x86'.
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
set CMAKE_PLATFORM=
set MSBUILD_PLATFORM=Win32

for %%A in (%*) do (
    if [%%A] EQU [Debug] (
        set CONFIG=%%A
    ) else if [%%A] EQU [Release] (
        set CONFIG=%%A
    ) else if [%%A] EQU [x86] (
        set PLATFORM=%%A
    ) else if [%%A] EQU [x64] (
        set PLATFORM=%%A
        set CMAKE_PLATFORM= Win64
        set MSBUILD_PLATFORM=x64
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
cmake -DCMAKE_BUILD_TYPE=%CONFIG% -G "Visual Studio 14 2015%CMAKE_PLATFORM%" -DTARGET_PLATFORM=%PLATFORM% -DAPP_VERSION=%APP_VERSION% ..\..
if %ERRORLEVEL% GEQ 1 goto build_error

msbuild /p:Configuration=%CONFIG% /p:Platform=%MSBUILD_PLATFORM% /m /t:build /ignore:.vcproj,.vcxproj
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
echo Usage: %0 [^<configuration^>] [^<platform^>] [rebuild^|clean]
echo        ^<configuration^> - Debug or Release
echo        ^<platform^>      - x86 or x64
echo        clean           - removes build directory
echo        rebuild         - performs clean and build
goto exit_error

:exit_success
exit /b 0
:exit_error
exit /b 1