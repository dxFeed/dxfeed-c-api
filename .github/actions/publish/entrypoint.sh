#!/bin/sh

set -euo pipefail

if [ "${GITHUB_REF}" == "${GITHUB_REF#refs/tags/}" ]; then
  echo "This is not a tagged push." 1>&2
  exit 78
fi

TAG="${GITHUB_REF#refs/tags/}"
echo "TAG: $TAG"

# Prepare the headers
# AUTH_HEADER="Authorization: token ${GITHUB_TOKEN}"

RELEASE_TITLE="$RELEASE_PREFIX$TAG"
echo "TITLE: $RELEASE_TITLE"

ls -1 $@

PRERELEASE_ARG=""
if [ -n "${INPUT_PRERELEASE_REGEX}" ]; then
  if echo "${TAG}" | grep -qE "$INPUT_PRERELEASE_REGEX"; then
    PRERELEASE_ARG="-prerelease"
  fi
fi

DRAFT_ARG=""
if [ -n "${INPUT_DRAFT_REGEX}" ]; then
  if echo "${TAG}" | grep -qE "$INPUT_DRAFT_REGEX"; then
    DRAFT_ARG="-draft"
  fi
fi

# recreate release
ghr -u "${GITHUB_REPOSITORY%/*}" -r "${GITHUB_REPOSITORY#*/}" $DRAFT_ARG $PRERELEASE_ARG -replace -delete -n "$RELEASE_TITLE" "$TAG" $@
