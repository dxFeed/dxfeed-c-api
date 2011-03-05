del binaries.zip
devenv DXFeed.sln /build release /project DXFeed
devenv DXFeed.sln /build release /project APITest
devenv DXFeed.sln /build release /project CommandLineSample
pkzipc -add -excl=.svn binaries.zip bin\pthreadVC2.* bin\APITest.exe bin\CommandLineSample.exe bin\DXFeed.lib bin\DXFeed.dll