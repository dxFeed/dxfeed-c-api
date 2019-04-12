#!/bin/sh

# Script CPack archives with specified platform and configuration. After
# packing all files extracted to single release directory.
# Usage:
#     combine_package <project-name> <configuration> <platform> <version> [no-tls]
# where:
#     project-name    - The name of target project package
#     configuration   - Debug or Release
#     platform        - x86 or x64
#     version         - version of application i.e. 1.2.6
#
# WARNING: you must set the next environment variables
#     PACKAGE_WORK_DIR - the working directory where cpack result arhive will be stored and unpacked

# Check cpack application in PATH
#rem Check cpack application in PATH
which cpack > /dev/null
if [ $? -ne 0 ]; then
    echo "The 'cpack' application is missing. Ensure it is installed and placed in your PATH."
    exit 31
fi

# Check 7z archiver in PATH
which 7z > /dev/null
if [ $? -ne 0 ]; then
    echo "The '7z' application is missing. Ensure it is installed and placed in your PATH."
    exit 32
fi

# Check PACKAGE_WORK_DIR environment variable
if [ "$PACKAGE_WORK_DIR" = "" ]; then
    echo "ERROR: Environment variable PACKAGE_WORK_DIR is not specified!"
    echo "Please set 'PACKAGE_WORK_DIR=...' in your caller script or command line."
    exit 33
fi

PROJECT_NAME=$1
CONFIG=$2
PLATFORM=$3
VERSION=$4
NO_TLS=$5
PACKAGE_SUFFIX=""

if [ "$PROJECT_NAME" = "" ]; then
    echo "ERROR: Project package name is not specified!"
    exit 34
fi

if [ ! "$CONFIG" = "Debug" ]; then
    if [ ! "$CONFIG" = "Release" ]; then
        echo "ERROR: Invalid configuration value '$CONFIG'"
        exit 35
    fi
fi

if [ ! "$PLATFORM" =  "x86" ]; then
    if [ ! "$PLATFORM" = "x64" ]; then
        echo "ERROR: Invalid platform value '$PLATFORM'"
        exit 36
    fi
fi

if [ "$NO_TLS" = "no-tls" ]; then
    PACKAGE_SUFFIX="-no-tls"
fi

cpack -G ZIP -C $CONFIG --config $PLATFORM/$CONFIG/DXFeedAllCPackConfig.cmake

if [ $? -ne 0 ]; then
    exit $?
fi

PACKAGE_NAME=$PROJECT_NAME-$VERSION-$PLATFORM$PACKAGE_SUFFIX
ARCHIVE_NAME=$PROJECT_NAME-$VERSION-$PLATFORM$CONFIG$PACKAGE_SUFFIX

mv -f $PACKAGE_NAME.zip $PACKAGE_WORK_DIR/$ARCHIVE_NAME.zip
7z x -y -o$PACKAGE_WORK_DIR $PACKAGE_WORK_DIR/$ARCHIVE_NAME.zip

if [ $? -ne 0 ]; then
    exit $?
fi

exit 0
