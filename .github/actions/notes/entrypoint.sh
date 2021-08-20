#!/bin/bash

set -euo pipefail

if [ "${GITHUB_REF}" == "${GITHUB_REF#refs/tags/}" ]; then
  echo "This is not a tagged push." 1>&2
  exit 78
fi

TAG="${GITHUB_REF#refs/tags/}"
echo "TAG: $TAG"

IFS=''
NL=$'\n'
RELEASE_NOTES=''
STARTFLAG="false"
while read LINE; do
    if [ "$STARTFLAG" == "true" ]; then
        if [[ "$LINE" == Version* ]];then
            break
        else
          #
          # only lines, starting with '*' or '-'
          # should be prepended with new-line character
          #
          if [ -n "$RELEASE_NOTES" ]; then
            if [[ "$LINE" =~ ^\*.*  ]]; then
              RELEASE_NOTES="${RELEASE_NOTES}${NL}"
            elif [[ "$LINE" =~ ^[[:space:]]*-.*  ]]; then
              RELEASE_NOTES="${RELEASE_NOTES}${NL}"
            fi
          fi
          RELEASE_NOTES="${RELEASE_NOTES}${LINE}"
        fi
    elif [[ "$LINE" == Version* ]]; then
        VERSION=$(echo "$LINE" | awk '{print $2}')
        if [[ "$VERSION" == "$TAG" ]]; then
            STARTFLAG="true"
        fi
        continue
    fi
done < "$INPUT_FILE"

echo ""
echo "=== RELEASE NOTES ================================"
echo "$RELEASE_NOTES"
echo "=================================================="
echo ""

if [ -n "$RELEASE_NOTES" ]; then
     RELEASE_NOTES=$(echo "$RELEASE_NOTES" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g; s/'"'"'/\&#39;/g' | sed 's/\n/<br\/>/g')
     RELEASE_NOTES="${RELEASE_NOTES//'%'/'%25'}"
     RELEASE_NOTES="${RELEASE_NOTES//$'\n'/'%0A'}"
     RELEASE_NOTES="${RELEASE_NOTES//$'\r'/'%0D'}"
    echo ::set-output name=data::"$RELEASE_NOTES"
else
    echo ::set-output name=data::""
fi
