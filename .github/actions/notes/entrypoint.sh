#!/bin/bash

set -euo pipefail

if [ "${GITHUB_REF}" == "${GITHUB_REF#refs/tags/}" ]; then
  echo "This is not a tagged push." 1>&2
  exit 78
fi

TAG="${GITHUB_REF#refs/tags/}"
echo "TAG: $TAG"

if [[ "$TAG" =~ ^([0-9]+.[0-9]+.[0-9]+)-[a-zA-Z]+$ ]]; then
  TAG="${BASH_REMATCH[1]}"
elif [[ "$TAG" =~ ^[a-zA-Z]+-([0-9]+.[0-9]+.[0-9]+)$ ]]; then
  TAG="${BASH_REMATCH[1]}"
fi

echo "TAG2: $TAG"

IFS=''
NL=$'\n'
RELEASE_NOTES=''
STARTFLAG="false"
while read LINE; do
    if [ "$STARTFLAG" == "true" ]; then
        if [[ "$LINE" == Version* ]];then
            break
        else
          if [ -n "$RELEASE_NOTES" ]; then
            RELEASE_NOTES="${RELEASE_NOTES}${NL}"
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
#    echo ::set-output name=data::"$RELEASE_NOTES"
    echo "data=$RELEASE_NOTES" >> $GITHUB_OUTPUT
else
#    echo ::set-output name=data::""
    echo "data=" >> $GITHUB_OUTPUT
fi
