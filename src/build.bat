@echo off
set BUILD_DIR=build
set CONFIG=
set PLATFORM=

if [%*] EQU 0 (
    goto build
) else if [%*] GTR 2 (
    goto usage
) else (
    for %%A in (%*) do (
        if %%A EQU "Debug" (
            if %CONGIG% NEQ "" goto usage
            set CONFIG=%%A
        ) else if %%A EQU "Release" (
            if %CONGIG% NEQ "" goto usage
            set CONFIG=%%A
        ) else if %%A EQU "x86" (
            if %PLATFORM% NEQ "" goto usage
            set PLATFORM=%%A
        ) else if %%A EQU "x64" (
            if %PLATFORM% NEQ "" goto usage
            set PLATFORM=%%A
        )
    )
)
if %CONGIG% EQU "" set CONFIG=Release
if %PLATFORM% EQU "" set PLATFORM=x86

echo Start building %CONFIG% %PLATFORM%...
exit 1

:build
if not exist %BUILD_DIR% (mkdir %BUILD_DIR%)
cd %BUILD_DIR%

REM cmake -DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 10 2010" ..

cmake -DCMAKE_BUILD_TYPE=Debug -G "NMake Makefiles" ..

cd..
echo Build success!
exit 1

:usage
echo Usage: %0 [configuration] [platform]
echo        configuration - Debug or Release
echo        platform      - x86 or x64
exit 1