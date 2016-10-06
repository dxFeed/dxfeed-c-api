@echo off
rem Script runs specified tests for various configurations and platforms
rem Run format:
rem     check_build <build_dir>
rem Where:
rem     build_dir - home direcory of the build
rem
rem Tests list is received from TESTS_LIST_FILE_NAME file which located at the
rem current directory. Each line of this file must contains only the name of 
rem test executable without postfixes, extension, parameters and any other 
rem symbols, e.g.:
rem ---------------- The content of TESTS_LIST_FILE_NAME ----------------------
rem |Test1                                                                    |
rem |Test2                                                                    |
rem |...                                                                      |
rem |TestN                                                                    |
rem -------------- The end of TESTS_LIST_FILE_NAME content---------------------

setlocal EnableDelayedExpansion

set BUILD_DIR=%1

rem Write list of runable tests in the next file
set TESTS_LIST_FILE_NAME=tests.list
set TESTS_LIST_FILE_PATH=%~dp0\%TESTS_LIST_FILE_NAME%
rem Write list of platforms here
rem Allowed platforms is x64 x86
set PLATFORMS=x64 x86
rem Write list of configurations here 
rem Allowed configurations is Debug Release
set CONFIGURATIONS=Debug Release

if NOT EXIST %TESTS_LIST_FILE_PATH% (
    echo ERROR: The tests list file '%TESTS_LIST_FILE_NAME%' not found^^!
    goto exit_error
)

for /F "tokens=*" %%T in (%TESTS_LIST_FILE_PATH%) do (
    for %%P in (%PLATFORMS%) do (
        for %%C in (%CONFIGURATIONS%) do (
            echo Test %%T on %%P %%C
            call %~dp0\run_test %BUILD_DIR% %%T %%P %%C
            if !ERRORLEVEL! GEQ 1 goto exit_error
        )
    )
)

:exit_success
echo Checking build success.
exit /b 0
:exit_error
echo Checking build failed!
exit /b 1