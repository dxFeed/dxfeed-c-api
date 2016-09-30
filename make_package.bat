@echo off
rem TODO
rem Script builds, checks and makes package
rem Script build all targets from CMakeLists.txt
rem It is sequentionally calls ../build.bat for next configurations: Debug x86, Release x86, Debug x64, Release x64
rem It one of configurations fail the process stopped.

setlocal

set VERSION=
set BUILD_DIR=build

for %%A in (%*) do (
    if [%%A] EQU [/^?] (
        goto usage
    ) else if [%%A] EQU [clean] (
        call build.bat clean
        goto end
    ) else if [%%A] EQU [rebuild] (    
        call build.bat clean
    ) else (
        set VERSION=%%A
    )
)

echo Start building package %VERSION%

rem === UPDATE VERSION ===
set APP_VERSION=%VERSION%

rem === BUILD ALL TARGETS ===

call build.bat Debug x86
if %ERRORLEVEL% GEQ 1 goto exit_error

call build.bat Release x86
if %ERRORLEVEL% GEQ 1 goto exit_error

call build.bat Debug x64
if %ERRORLEVEL% GEQ 1 goto exit_error

call build.bat Release x64
if %ERRORLEVEL% GEQ 1 goto exit_error

rem === TEST BUILDS ===

echo Start checking build %VERSION%
call %~dp0\scripts\check_build %BUILD_DIR%
if %ERRORLEVEL% GEQ 1 goto exit_error

rem === TEST BUILDS ===

echo Start make release package %VERSION%


rem === FINISH ===
echo Making package complete successfully.
goto exit_success

:usage
echo Usage: %0 [^<major^>.^<minor^>[.^<patch^>]] [rebuild^|clean]
echo        [^<major^>.^<minor^>[.^<patch^>]] - Version of package, i.e. 1.2.6
echo        clean                       - removes build directory
echo        rebuild                     - performs clean and build
goto exit_success

:exit_success
exit /b 0
:exit_error
echo Making package failed!
exit /b 1
:end