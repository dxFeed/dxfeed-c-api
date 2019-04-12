#!/bin/sh

# This script runs building project using CMakeLists.txt from current folder.
# The result of building places into BUILD_DIR directory.
# Usage: 
#     build.sh [<configuration>] [<platform>] [rebuild|clean] [no-tls]
# Where
#     <configuration> - Debug or Release
#     <platform>      - x86 or x64
#     clean           - removes build directory
#     rebuild         - performs clean and build
# The default configuration is 'Release x64'.
#
# WARNING: you can set the next environment variables
#     APP_VERSION - the version of target in format major[.minor[.patch]], i.e. 1.2.6
#                   If you want to specify version you must set 'set APP_VERSION=...'
#                   in your caller script or command line. Otherwise the default version 
#                   from sources will be used.

print_usage() {
    echo "Usage: $0 [<configuration>] [<platform>] [rebuild|clean]"
    echo "       <configuration> - Debug or Release"
    echo "       <platform>      - x86 or x64"
    echo "       clean           - removes build directory"
    echo "       rebuild         - performs clean and build"
}

BUILD_DIR="$(pwd)/build"
CONFIG=""
PLATFORM=""
DISABLE_TLS="OFF"

for A in "$@"; do
    if [ "$A" = "Debug" ]; then
        CONFIG=$A
    elif [ "$A" = "Release" ]; then
        CONFIG=$A
    elif [ "$A" = "x86" ]; then
        PLATFORM=$A
    elif [ "$A" = "x64" ]; then
        PLATFORM=$A
    elif [ "$A" = "clean" ]; then
        if [ -d $BUILD_DIR ]; then
            rm -rf $BUILD_DIR

            if [ $? -ne 0 ]; then
                exit 11
            fi
        fi

        exit 0
    elif [ "$A" = "rebuild" ]; then
        if [ -d $BUILD_DIR ]; then
            rm -rf $BUILD_DIR

            if [ $? -ne 0 ]; then
                exit 12
            fi
        fi
    elif [ "$A" = "no-tls" ]; then
        DISABLE_TLS="ON"
    else
        print_usage
        exit 13
    fi
done

if [ "$CONFIG" = "" ]; then
    CONFIG="Release"
fi

if [ "$PLATFORM" = "" ]; then
    PLATFORM="x64"
fi

BUILD_DIR=$BUILD_DIR/$PLATFORM/$CONFIG

if [ ! -d $BUILD_DIR ]; then
    mkdir -p $BUILD_DIR
fi

cd $BUILD_DIR

echo "Start building $CONFIG $PLATFORM..."
cmake -DCMAKE_BUILD_TYPE=$CONFIG -G "Unix Makefiles" -DDISABLE_TLS=$DISABLE_TLS -DTARGET_PLATFORM=$PLATFORM -DAPP_VERSION=$APP_VERSION ../../..

if [ $? -ne 0 ]; then
    cd ../..
    echo "Build error!"
    exit 13
fi

make

if [ $? -ne 0 ]; then
    cd ../..
    echo "Build error!"
    exit 14
fi

cd ../..
echo "Build success."
exit 0
