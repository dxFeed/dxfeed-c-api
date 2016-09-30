@echo off
rem Script runs specified tests for various configurations and platforms
rem Run format:
rem     check_build <build_dir>
rem Where:
rem     build_dir - home direcory of the build

setlocal EnableDelayedExpansion

set BUILD_DIR=%1

rem Write list of runable tests here
set TESTS=FullTest
rem Write list of platforms here
rem Allowed platforms is x64 x86
set PLATFORMS=x64 x86
rem Write list of configurations here 
rem Allowed configurations is Debug Release
set CONFIGURATIONS=Debug Release

for %%T in (%TESTS%) do (
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