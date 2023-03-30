FROM debian:stable-slim as base

RUN apt-get update && \
    apt-get install -y jq curl go && \
    apt-get clean && \
    rm -rf "/var/lib/apt/lists/*"

LABEL "com.github.actions.name"="github-action-release-publish"
LABEL "com.github.actions.description"="Creates release for tag in repository"
LABEL "com.github.actions.icon"="settings"
LABEL "com.github.actions.color"="gray-dark"

LABEL version="1.0.0"
LABEL repository="http://github.com/mvkvl/github-upload-release-artifacts-action"
LABEL homepage="http://github.com/mvkvl/github-upload-release-artifacts-action"

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN mkdir -p /opt/tools/go && \
    export GOPATH=/opt/tools/go && \
    export PATH=$GOPATH/bin:$PATH && \
    go install github.com/tcnksm/ghr@latest

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
