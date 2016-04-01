set BUILD_DIR=build

if not exist %BUILD_DIR% (mkdir %BUILD_DIR%)
cd %BUILD_DIR%

cmake -DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 10 2010" ..

cd..