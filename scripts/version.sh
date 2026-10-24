#!/bin/sh
# Set the version of the app in each place that holds it.
# Usage: scripts/version.sh X.Y.Z
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
V=${1:?usage: scripts/version.sh X.Y.Z}
for plist in "$ROOT/platform/ios6/Info.plist" "$ROOT/platform/macos/Info.plist"; do
    plutil -replace CFBundleShortVersionString -string "$V" "$plist"
    plutil -replace CFBundleVersion -string "$V" "$plist"
done
sed -i '' "s/MARKETING_VERSION: \"[0-9.]*\"/MARKETING_VERSION: \"$V\"/" "$ROOT/platform/ios/project.yml"
sed -i '' "s/#define FERN_APP_VERSION \"[0-9.]*\"/#define FERN_APP_VERSION \"$V\"/" "$ROOT/core/fern_version.h"
echo "$V"
