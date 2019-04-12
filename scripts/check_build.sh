#!/bin/sh

# Script runs specified tests for various configurations and platforms
# Run format:
#     check_build <build_dir>
# Where:
#     build_dir - home direcory of the build
#
# Tests list is received from TESTS_LIST_FILE_NAME file which located at the
# current directory. Each line of this file must contains only the name of 
# test executable without postfixes, extension, parameters and any other 
# symbols, e.g.:
# ---------------- The content of TESTS_LIST_FILE_NAME ----------------------
# |Test1                                                                    |
# |Test2                                                                    |
# |...                                                                      |
# |TestN                                                                    |
# -------------- The end of TESTS_LIST_FILE_NAME content---------------------

BUILD_DIR="$1"

# Write list of runable tests in the next file
TESTS_LIST_FILE_NAME="tests.list"
TESTS_LIST_FILE_PATH="$TESTS_LIST_FILE_NAME"
#Write list of platforms here
#Allowed platforms is x64 x86
#PLATFORMS="x64 x86"
PLATFORMS="x64"
#rem Write list of configurations here 
#rem Allowed configurations is Debug Release
#CONFIGURATIONS="Debug Release"
CONFIGURATIONS="Release"

if [ ! -f $TESTS_LIST_FILE_PATH ]; then
    echo "ERROR: The tests list file '$TESTS_LIST_FILE_NAME' not found!"
    echo "Checking build failed!"
    exit 21
fi

while read T; do
    for P in $PLATFORMS; do
        for C in $CONFIGURATIONS; do
            echo "Test $T on $P $C"
            ./run_test.sh $BUILD_DIR $T $P $C
            if [ $? -ne 0 ]; then
                echo "Checking build failed!"
                exit $?
            fi
        done
    done
done < $TESTS_LIST_FILE_PATH

echo Checking build success.
exit 0
