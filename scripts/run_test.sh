#!/bin/sh

# Run specified executable in format
#     run_test <build_dir> <executable> <platform> <configuration>
# Where:
#     build_dir     - home direcory of the build
#     executable    - executable name without postfixes and extension
#     platform      - one of following: x86 or x64
#     configuration - one of following: Debug or Release
# 
# Also script reads arguments for executable from file '<executable>.args'.
# If file with such name is not exist at the same folder, executable will
# be started without any arguments. File '<executable>.args' is reads line
# by line. Each line of this file considered as set-of-arguments. Empty 
# lines will be skipped.

BUILD_DIR="$1"
EXEC="$2"
PLATFORM="$3"
CONFIGURATION="$4"
PLATFORM_POSTFIX=""

if [ "$PLATFORM" = "x64" ]; then
    PLATFORM_POSTFIX="_64"
fi

CONF_POSTFIX=""

if [ "$CONFIGURATION" = "Debug" ]; then 
    CONF_POSTFIX="d"
fi

POSTFIX="$CONF_POSTFIX$PLATFORM_POSTFIX"
WORKING_DIR="$BUILD_DIR/$PLATFORM/$CONFIGURATION"
EXEC_FULL_PATH="$WORKING_DIR/$EXEC$POSTFIX"
ARGS_FILE="$EXEC.args"

if [ ! -f $EXEC_FULL_PATH ]; then
    echo "ERROR: The test executable '$EXEC$POSTFIX' not found! Abort testing"
    exit 101
fi

if [ -f $ARGS_FILE ]; then
    while read line; do
        echo "Run $EXEC$POSTFIX $line"
        $EXEC_FULL_PATH $line
        
        if [ $? -ne 0 ]; then
            exit $?
        fi
    done < $ARGS_FILE
else
    $EXEC_FULL_PATH
fi

exit 0