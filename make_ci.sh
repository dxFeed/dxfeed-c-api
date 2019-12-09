#!/bin/sh

TAG="$(git describe --tags --abbrev=0)"

./make_package.sh "$TAG" "$@"