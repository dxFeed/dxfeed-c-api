@echo off
rem Run specified executable in format
rem     run_test <build_dir> <executable> <platform> <configuration>
rem Where:
rem     build_dir     - home direcory of the build
rem     executable    - executable name without postfixes and extension
rem     platform      - one of following: x86 or x64
rem     configuration - one of following: Debug or Release

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
%WORKING_DIR%\%EXEC%%POSTFIX%
exit /b %errorlevel%
