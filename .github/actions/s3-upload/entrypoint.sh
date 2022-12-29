#!/bin/sh

set -e

#echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
#echo "KEY   : $(echo $AWS_ACCESS_KEY_ID     | sed 's|.|& |g')"
#echo "SECRET: $(echo $AWS_SECRET_ACCESS_KEY | sed 's|.|& |g')"
#echo "REGION: $(echo $AWS_DEFAULT_REGION    | sed 's|.|& |g')"
#echo "BUCKET: $(echo $AWS_S3_BUCKET         | sed 's|.|& |g')"
#echo "PATH  : $(echo $AWS_S3_PATH           | sed 's|.|& |g')"
#echo "SOURCE: $(echo $SOURCE_DIR            | sed 's|.|& |g')"
#echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

aws configure list
#aws s3 ls s3://${AWS_S3_BUCKET} --recursive
aws s3 rm s3://${AWS_S3_BUCKET}${AWS_S3_PATH} --recursive
aws s3 sync ${SOURCE_DIR} s3://${AWS_S3_BUCKET}${AWS_S3_PATH}
