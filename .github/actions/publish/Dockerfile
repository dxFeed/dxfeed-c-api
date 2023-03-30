FROM debian:stable-slim as base

RUN apt-get update && \
    apt-get install -y jq curl && \
    apt-get clean && \
    rm -rf "/var/lib/apt/lists/*"

LABEL "com.github.actions.name"="github-action-release-publish"
LABEL "com.github.actions.description"="Creates release for tag in repository"
LABEL "com.github.actions.icon"="settings"
LABEL "com.github.actions.color"="gray-dark"

LABEL version="1.0.0"
LABEL repository="http://github.com/mvkvl/github-upload-release-artifacts-action"
LABEL homepage="http://github.com/mvkvl/github-upload-release-artifacts-action"

RUN curl -s https://api.github.com/repos/tcnksm/ghr/releases/latest | \
    jq -r '.assets[] | select(.browser_download_url | contains("linux_amd64"))  | .browser_download_url' | \
    xargs curl -o ghr.tgz -sSL && \
    mkdir -p ghr && \
    tar xzf ghr.tgz && \
    mv ghr_v*_linux_amd64/ghr /usr/local/bin && \
    rm -rf ghr*

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
