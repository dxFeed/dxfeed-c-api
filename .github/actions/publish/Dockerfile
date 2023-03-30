FROM alpine:latest as base

LABEL "com.github.actions.name"="github-action-release-publish"
LABEL "com.github.actions.description"="Creates release for tag in repository"
LABEL "com.github.actions.icon"="settings"
LABEL "com.github.actions.color"="gray-dark"

LABEL version="1.0.0"
LABEL repository="http://github.com/mvkvl/github-upload-release-artifacts-action"
LABEL homepage="http://github.com/mvkvl/github-upload-release-artifacts-action"

RUN apk add --no-cache jq curl go

SHELL ["/bin/ash", "-o", "pipefail", "-c"]

RUN mkdir -p /opt/tools/go && \
    export GOPATH=/opt/tools/go && \
    export PATH=$GOPATH/bin:$PATH && \
    go install github.com/tcnksm/ghr@latest

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
