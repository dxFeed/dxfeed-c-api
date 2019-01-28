@if "%VCDIR%" == "" @goto environment
@set path=%path%;C:/program/winzip;%VCDIR%/../Common7/IDE;%VCDIR%/bin

echo %path%


del binaries.zip
devenv DXFeed.sln /build release /project DXFeed
devenv DXFeed.sln /build release /project APITest
devenv DXFeed.sln /build release /project CommandLineSample
C:/program/winzip/wzzip -add -excl=.svn binaries.zip bin\pthreadVC2.* bin\APITest.exe bin\CommandLineSample.exe bin\DXFeed.lib bin\DXFeed.dll
@if errorlevel 1 goto failure
@echo.
@goto success


:environment
@echo Error: VCDIR environment variable is not defined. Please define it and start again.
@echo Info:  VCDIR must contain path to MS Visual C++ (.Net 2003) installation directory (Ex: c:\msvs\vc7)

:failure
@echo off
cd ..
@echo Build failed.
@echo.
@exit /B 1

:success
@echo off
cd ..
@echo Build succeeded.
@echo.
@exit /B 0

