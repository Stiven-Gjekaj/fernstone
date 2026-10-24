#!/bin/sh
# Put the version manifests of manifests/ into core/manifest_data.h, so that
# the app has them with no file to copy.
# Usage: scripts/manifests.sh
# Run it after each change in manifests/. Git tracks the result.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/core/manifest_data.h"
{
    echo "// manifest_data.h: made by scripts/manifests.sh from manifests/*.json."
    echo "// Do not edit it. Edit the JSON files and run the script again."
    echo "static const char *const fern_manifest_json[] = {"
    for f in "$ROOT"/manifests/*.json; do
        printf '    "'
        tr '\n' ' ' <"$f" | sed 's/\\/\\\\/g; s/"/\\"/g; s/  */ /g; s/ $//'
        printf '",\n'
    done
    echo "    0"
    echo "};"
} >"$OUT"
echo "$OUT"
