FROM alpine:3.9 as base

RUN apk add --no-cache jq curl bash

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
