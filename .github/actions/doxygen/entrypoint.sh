#!/bin/bash

export DOC_ARCH_NAME="${INPUT_ARTIFACT}"
export BUILD_VERSION="${INPUT_RELEASE}"

mkdir -p artifact
sed -i "s|/src/c-api|$(pwd)|g" ./docs/Doxyfile.release
cd docs
doxygen Doxyfile.release

cd $DOC_ARCH_NAME
mv html $DOC_ARCH_NAME

zip -r $DOC_ARCH_NAME.zip $DOC_ARCH_NAME
ls -lh .
mv $DOC_ARCH_NAME.zip ../../artifact/
