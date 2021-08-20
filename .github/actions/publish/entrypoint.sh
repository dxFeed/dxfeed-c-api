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

#
# update release notes
#
if [ -n "${INPUT_NOTES}" ]; then
    echo "--- json ------------------------------------------"
    DATA=$(jq -n --arg message "$INPUT_NOTES" '{"body":$message}')
    echo "$DATA"

    RELEASE_ID=$(curl --silent -X 'GET' https://api.github.com/repos/${GITHUB_REPOSITORY%/*}/${GITHUB_REPOSITORY#*/}/releases/tags/${TAG} | jq ".id")
    echo "RELEASE_ID: $RELEASE_ID"

    echo "API URL: https://api.github.com/repos/${GITHUB_REPOSITORY%/*}/${GITHUB_REPOSITORY#*/}/releases/$RELEASE_ID?access_token=${GITHUB_TOKEN}"
    curl --silent -X 'PATCH' https://api.github.com/repos/${GITHUB_REPOSITORY%/*}/${GITHUB_REPOSITORY#*/}/releases/$RELEASE_ID?access_token=${GITHUB_TOKEN} -d "$DATA"
else
  echo ""
  echo "Release notes not found for $TAG. Won't update release description."
  echo ""
fi
