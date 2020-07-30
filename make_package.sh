#!/bin/sh

# Script builds, tests and makes package.
# Script build all targets from CMakeLists.txt by sequentionally calling 
# build.sh for next configurations: Debug x86, Release x86, Debug x64, Release x64. 
# If one of configurations fail the process stopped.
# Usage: 
#     make_package <major>.<minor>[.<patch>] [rebuild|clean] [no-test] [no-tls]
# Where
#     <major>.<minor>[.<patch>] - Version of package, i.e. 1.2.6
#     clean                     - removes build directory
#     rebuild                   - performs clean and build
#     no-test                   - build testing will not be started
# 
# The operation order:
#     1. Build sources in next configurations Debug x86, Release x86, 
#        Debug x64, Release x64. The result of build is located into the 
#        BUILD_DIR directory. Building performes with build.sh script from
#        current directory where this script is located.
#     2. Run tests of build with scripts\check_build.sh.
#     3. Create individual packages for each configuration with 
#        scripts\combine_package.sh. Then create the target package which
#        will be placed into BUILD_DIR directory.
# Dependencies:
#     1. CMake x64 + CPack
#     2. 7z

print_usage() {
    echo "Usage: $0 <major>.<minor>[.<patch>] [rebuild|clean] [no-test] [no-tls]"
    echo "   <major>.<minor>[.<patch>] - Version of package, i.e. 1.2.6"
    echo "   clean                     - removes build directory"
    echo "   rebuild                   - performs clean and build"
    echo "   no-test                   - build tests will not be started"
    echo "   no-tls                    - build without TLS support"
}

# Check cmake application in PATH
which cmake > /dev/null
if [ $? -ne 0 ]; then
    echo "The 'cmake' application is missing. Ensure it is installed and placed in your PATH."
    exit 1
fi

#rem Check cpack application in PATH
which cpack > /dev/null
if [ $? -ne 0 ]; then
    echo "The 'cpack' application is missing. Ensure it is installed and placed in your PATH."
    exit 2
fi

# Check 7z archiver in PATH
which 7z > /dev/null
if [ $? -ne 0 ]; then
    echo "The '7z' application is missing. Ensure it is installed and placed in your PATH."
    exit 3
fi

VERSION="$1"
DO_TEST=1
BUILD_DIR="$(pwd)/build"
PROJECT_NAME="DXFeedAll"
PACKAGE_WORK_DIR="_CPack_Packages"
TARGET_PACKAGE="dxfeed-c-api-$VERSION"
NO_TLS=""
PACKAGE_SUFFIX=""
#PLATFORMS="x64 x86"
PLATFORMS="x64"
CONFIGURATIONS="Debug Release"
#CONFIGURATIONS="Release"

for A in "$@"; do
    if [ "$A" = "clean" ]; then
        ./build.sh clean
        exit 0
    elif [ "$A" = "rebuild" ]; then
        ./build.sh clean
    elif [ "$A" = "no-test" ]; then
        DO_TEST=0
    elif [ "$A" = "no-tls" ]; then
        NO_TLS="no-tls"
        PACKAGE_SUFFIX="-no-tls"
    fi
done

# Check version parameter
if [ "$VERSION" = "" ]; then
    echo "ERROR: The version of package is not specified or invalid!"
    print_usage
    echo "Making package failed! $?"
    exit 4
fi

echo "Start building package $VERSION"

# === UPDATE VERSION ===
APP_VERSION=$VERSION

# === BUILD ALL TARGETS ===

export APP_VERSION

for P in $PLATFORMS; do
    for C in $CONFIGURATIONS; do
        echo "./build.sh $C $P $NO_TLS"
    
        ./build.sh $C $P $NO_TLS

        if [ $? -ne 0 ]; then
            echo "Making package failed! $?"
            exit $?
        fi
    done
done

# === TEST BUILDS ===

if [ $DO_TEST -eq 1 ]; then
    echo "Start checking build $VERSION"
    scripts/check_build.sh $BUILD_DIR
    
    if [ $? -ne 0 ]; then
        echo "Making package failed! $?"
        exit $?
    fi
else
    echo "Build checking will be skipped."
fi

# === MAKE PACKAGE ===

echo "Start make release package $VERSION"
HOME_DIR=$(pwd)
cd $BUILD_DIR

export PACKAGE_WORK_DIR

for P in $PLATFORMS; do
    for C in $CONFIGURATIONS; do
        echo "$HOME_DIR/scripts/combine_package.sh $PROJECT_NAME $C $P $VERSION $NO_TLS $PACKAGE_WORK_DIR"
        $HOME_DIR/scripts/combine_package.sh $PROJECT_NAME $C $P $VERSION $NO_TLS

        if [ $? -ne 0 ]; then
            cd $HOME_DIR
            echo "Making package failed! $?"
            exit $?
        fi
    done
done

if [ ! -d $PACKAGE_WORK_DIR ]; then
    mkdir $PACKAGE_WORK_DIR
fi

cd $PACKAGE_WORK_DIR

if [ -d $TARGET_PACKAGE$PACKAGE_SUFFIX ]; then
    rm -rf $TARGET_PACKAGE$PACKAGE_SUFFIX
fi

mkdir $TARGET_PACKAGE$PACKAGE_SUFFIX
for P in $PLATFORMS; do
    cp -rf $PROJECT_NAME-$VERSION-$P$PACKAGE_SUFFIX $TARGET_PACKAGE$PACKAGE_SUFFIX
done

7z a $TARGET_PACKAGE$PACKAGE_SUFFIX.zip $TARGET_PACKAGE$PACKAGE_SUFFIX
mv -f $TARGET_PACKAGE$PACKAGE_SUFFIX.zip $BUILD_DIR/$TARGET_PACKAGE$PACKAGE_SUFFIX.zip
cd $HOME_DIR

# === FINISH ===

echo "Making package complete successfully."
exit 0
