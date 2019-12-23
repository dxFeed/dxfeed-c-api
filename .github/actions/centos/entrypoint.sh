#!/bin/bash

export RELEASE=$INPUT_RELEASE
export ARTIFACT=$INPUT_ARTIFACT
export NOTLS=$INPUT_NOTLS

NO_TLS_FLAG=""
NO_TLS_SUFFIX=""

if $NOTLS ; then
  NO_TLS_FLAG="no-tls"
  NO_TLS_SUFFIX="-${NO_TLS_FLAG}"
fi

ARTIFACT="${ARTIFACT}${NO_TLS_SUFFIX}"

echo "FLAGS: $RELEASE rebuild no-test $NO_TLS_FLAG"
echo "ARCH : $ARTIFACT"

mkdir -p artifact
source scl_source enable devtoolset-2
./make_package.sh $RELEASE rebuild no-test $NO_TLS_FLAG
mv ./build/dxfeed-c-api-$RELEASE${NO_TLS_SUFFIX}.zip ./artifact/$ARTIFACT.zip

#ARCH_NAME=dxfeed-c-api-$BUILD_VERSION-centos
#git clone https://github.com/dxFeed/dxfeed-c-api.git $SRC_PATH                 && \
#source scl_source enable devtoolset-2                                          && \
#cd $SRC_PATH                                                                   && \
#./make_package.sh $BUILD_VERSION rebuild no-test no-tls                        && \
#cp ./build/dxfeed-c-api-$BUILD_VERSION-no-tls.zip /build/$ARCH_NAME-no-tls.zip && \
#./make_package.sh $BUILD_VERSION rebuild no-test                               && \
#cp ./build/dxfeed-c-api-$BUILD_VERSION.zip /build/$ARCH_NAME.zip