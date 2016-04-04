@echo off

set BUILD_DIR=build
set CONFIG=
set PLATFORM=
set CMAKE_PLATFORM=

for %%A in (%*) do (
    if [%%A] EQU [Debug] (
        set CONFIG=%%A
    ) else if [%%A] EQU [Release] (
        set CONFIG=%%A
    ) else if [%%A] EQU [x86] (
        set PLATFORM=%%A
    ) else if [%%A] EQU [x64] (
        set PLATFORM=%%A
        set CMAKE_PLATFORM=" Win64"
    ) else (
        goto usage
    )
)

:build
if [%CONFIG%] EQU [] set CONFIG=Release
if [%PLATFORM%] EQU [] set PLATFORM=x86

if not exist %BUILD_DIR% (mkdir %BUILD_DIR%)
cd %BUILD_DIR%

echo Start building %CONFIG% %PLATFORM%...

cmake -DCMAKE_BUILD_TYPE=%CONFIG% -G "NMake Makefiles%CMAKE_PLATFORM%" ..
if %ERRORLEVEL% GEQ 1 goto build_error

nmake
if %ERRORLEVEL% GEQ 1 goto build_error

:build_success
cd..
echo Build success.
goto end
:build_error
cd..
echo Build error!
goto end

:usage
echo Usage: %0 [configuration] [platform]
echo        configuration - Debug or Release
echo        platform      - x86 or x64

:end