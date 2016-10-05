@echo off
rem Run specified executable in format
rem     run_test <build_dir> <executable> <platform> <configuration>
rem Where:
rem     build_dir     - home direcory of the build
rem     executable    - executable name without postfixes and extension
rem     platform      - one of following: x86 or x64
rem     configuration - one of following: Debug or Release
rem 
rem Also script reads arguments for executable from file '<executable>.args'.
rem If file with such name is not exist at the same folder, executable will
rem be started without any arguments. File '<executable>.args' is reads line
rem by line. Each line of this file considered as set-of-arguments. Empty 
rem lines will be skipped. 

setlocal

set BUILD_DIR=%1
set EXEC=%2
set PLATFORM=%3
set CONFIGURATION=%4

set PLATFORM_POSTFIX=
if [%PLATFORM%] EQU [x64] set PLATFORM_POSTFIX=_64
set CONF_POSTFIX=
if [%CONFIGURATION%] EQU [Debug] set CONF_POSTFIX=d
set POSTFIX=%CONF_POSTFIX%%PLATFORM_POSTFIX%
set WORKING_DIR=%BUILD_DIR%\%PLATFORM%\%CONFIGURATION%
set ARGS_FILE=%~dp0\%EXEC%.args

if EXIST %ARGS_FILE% (
    for /F "tokens=*" %%A in (%ARGS_FILE%) do (
        echo Run %EXEC%%POSTFIX% %%A
        %WORKING_DIR%\%EXEC%%POSTFIX% %%A
        if %errorlevel% GEQ 1 exit /b %errorlevel%
    )
) else (
    %WORKING_DIR%\%EXEC%%POSTFIX%
)
exit /b %errorlevel%
