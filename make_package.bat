@echo off
rem TODO
rem Script builds, checks and makes package
rem Script build all targets from CMakeLists.txt
rem It is sequentionally calls ../build.bat for next configurations: Debug x86, Release x86, Debug x64, Release x64
rem It one of configurations fail the process stopped.

setlocal

rem Check cmake application in PATH
where /q cmake
if %ERRORLEVEL% GEQ 1 (
    echo The 'cmake' application is missing. Ensure it is installed and placed in your PATH.
    goto exit_error
)
rem Check msbuild application in PATH
where /q msbuild
if %ERRORLEVEL% GEQ 1 (
    echo The 'msbuild' application is missing. Ensure it is installed and placed in your PATH.
    goto exit_error
)
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

set VERSION=
set BUILD_DIR=build
set DO_TEST=1

for %%A in (%*) do (
    if [%%A] EQU [/^?] (
        goto usage
    ) else if [%%A] EQU [clean] (
        call build.bat clean
        goto end
    ) else if [%%A] EQU [rebuild] (
        call build.bat clean
    ) else if [%%A] EQU [no-test] (
        set DO_TEST=0
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

if %DO_TEST% GTR 0 (
    echo Start checking build %VERSION%
    call %~dp0\scripts\check_build %BUILD_DIR%
    if %ERRORLEVEL% GEQ 1 goto exit_error
) else (
    echo Checking will be skipped.
)

rem === MAKE PACKAGE ===

echo Start make release package %VERSION%
set HOME_DIR=%cd%
cd %BUILD_DIR%
call %~dp0\scripts\combine_package Debug x86 %VERSION%
set CPACK_LEVEL=%ERRORLEVEL%
call %~dp0\scripts\combine_package Release x86 %VERSION%
set CPACK_LEVEL=%CPACK_LEVEL%+%ERRORLEVEL%
call %~dp0\scripts\combine_package Debug x64 %VERSION%
set CPACK_LEVEL=%CPACK_LEVEL%+%ERRORLEVEL%
call %~dp0\scripts\combine_package Release x64 %VERSION%
set CPACK_LEVEL=%CPACK_LEVEL%+%ERRORLEVEL%

set TARGET_PACKAGE=dxfeed-c-api-%VERSION%
if EXIST %TARGET_PACKAGE% rmdir %TARGET_PACKAGE% /S /Q
mkdir %TARGET_PACKAGE%
xcopy /Y /S DXFeedAll-%VERSION%-x86 %TARGET_PACKAGE%
xcopy /Y /S DXFeedAll-%VERSION%-x64 %TARGET_PACKAGE%
7z a %TARGET_PACKAGE%.zip %TARGET_PACKAGE%
cd %HOME_DIR%
if %CPACK_LEVEL% GEQ 1 goto exit_error

rem === FINISH ===
echo Making package complete successfully.
goto exit_success

:usage
echo Usage: %0 [^<major^>.^<minor^>[.^<patch^>]] [rebuild^|clean] [no-test]
echo    [^<major^>.^<minor^>[.^<patch^>]] - Version of package, i.e. 1.2.6
echo    clean                       - removes build directory
echo    rebuild                     - performs clean and build
echo    no-test                     - build tests will not be started
goto exit_success

:exit_success
exit /b 0
:exit_error
echo Making package failed!
exit /b 1
:end