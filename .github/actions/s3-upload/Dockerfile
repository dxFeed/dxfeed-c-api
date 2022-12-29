FROM alpine:latest as base

LABEL "com.github.actions.name"="github-action-aws-s3-upload"
LABEL "com.github.actions.description"="Uploads files to AWS S3"
LABEL "com.github.actions.icon"="settings"
LABEL "com.github.actions.color"="gray-dark"

LABEL version="1.0.0"

RUN apk add --no-cache python3 py3-pip        && \
        pip3 install --upgrade pip            && \
        pip3 install --no-cache-dir awscli    && \
        rm -rf /var/cache/apk/*
RUN aws --version

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
