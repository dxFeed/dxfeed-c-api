@echo off
rem Script builds, tests and makes package.
rem Script build all targets from CMakeLists.txt by sequentionally calling 
rem build.bat for next configurations: Debug x86, Release x86, Debug x64, 
rem Release x64. If one of configurations fail the process stopped.
rem Usage: 
rem     make_package <major>.<minor>[.<patch>] [rebuild|clean] [no-test]
rem Where
rem     <major>.<minor>[.<patch>] - Version of package, i.e. 1.2.6
rem     clean                     - removes build directory
rem     rebuild                   - performs clean and build
rem     no-test                   - build testing will not be started
rem 
rem The operation order:
rem     1. Build sources in next configurations Debug x86, Release x86, 
rem        Debug x64, Release x64. The result of build is located into the 
rem        BUILD_DIR directory. Building performes with build.bat script from
rem        current directory where this script is located.
rem     2. Run tests of build with scripts\check_build.bat.
rem     3. Create individual packages for each configuration with 
rem        scripts\combine_package.bat. Then create the target package which
rem        will be placed into BUILD_DIR directory.
rem Dependencies:
rem     1. CMake x64 + CPack
rem     2. MSBuild
rem     3. MSBuild Command Prompt x64
rem     4. 7z

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

set VERSION=%1
set DO_TEST=1
set BUILD_DIR=%~dp0\build
set PROJECT_NAME=DXFeedAll
set PACKAGE_WORK_DIR=_CPack_Packages
set TARGET_PACKAGE=dxfeed-c-api-%VERSION%

for %%A in (%*) do (
    if [%%A] EQU [clean] (
        call build.bat clean
        goto end
    ) else if [%%A] EQU [rebuild] (
        call build.bat clean
    ) else if [%%A] EQU [no-test] (
        set DO_TEST=0
    )
)

rem Check version parameter
if [%VERSION%] EQU [] (
    echo ERROR: The version of package is not specified or invalid^!
    goto usage
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

if %DO_TEST% EQU 0 (
    echo Build checking will be skipped.
    goto make_package
)
echo Start checking build %VERSION%
call %~dp0\scripts\check_build %BUILD_DIR%
if %ERRORLEVEL% GEQ 1 goto exit_error

rem === MAKE PACKAGE ===

:make_package
echo Start make release package %VERSION%
set HOME_DIR=%cd%
cd %BUILD_DIR%
call %~dp0\scripts\combine_package %PROJECT_NAME% Debug x86 %VERSION%
if %ERRORLEVEL% GEQ 1 goto cpack_error
call %~dp0\scripts\combine_package %PROJECT_NAME% Release x86 %VERSION%
if %ERRORLEVEL% GEQ 1 goto cpack_error
call %~dp0\scripts\combine_package %PROJECT_NAME% Debug x64 %VERSION%
if %ERRORLEVEL% GEQ 1 goto cpack_error
call %~dp0\scripts\combine_package %PROJECT_NAME% Release x64 %VERSION%
if %ERRORLEVEL% GEQ 1 goto cpack_error

if NOT EXIST %PACKAGE_WORK_DIR% mkdir %PACKAGE_WORK_DIR%
cd %PACKAGE_WORK_DIR%
if EXIST %TARGET_PACKAGE% rmdir %TARGET_PACKAGE% /S /Q
mkdir %TARGET_PACKAGE%
xcopy /Y /S %PROJECT_NAME%-%VERSION%-x86 %TARGET_PACKAGE%
xcopy /Y /S %PROJECT_NAME%-%VERSION%-x64 %TARGET_PACKAGE%
7z a %TARGET_PACKAGE%.zip %TARGET_PACKAGE%
move /Y %TARGET_PACKAGE%.zip %BUILD_DIR%\%TARGET_PACKAGE%.zip
cd %HOME_DIR%

rem === FINISH ===
goto exit_success

:usage
echo Usage: %0 ^<major^>.^<minor^>[.^<patch^>] [rebuild^|clean] [no-test]
echo    ^<major^>.^<minor^>[.^<patch^>] - Version of package, i.e. 1.2.6
echo    clean                     - removes build directory
echo    rebuild                   - performs clean and build
echo    no-test                   - build tests will not be started
goto exit_error

:exit_success
echo.
echo Making package complete successfully.
exit /b 0
:cpack_error
cd %HOME_DIR%
goto exit_error
:exit_error
echo.
echo Making package failed!
exit /b 1
:end