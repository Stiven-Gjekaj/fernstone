#!/bin/sh
# Build the iOS app with ANGLE.
# Usage: scripts/ios.sh [--sim | --ipa]
# --sim builds for the simulator into build/ios-dd. --ipa builds an unsigned
# build/Fernstone.ipa. SideStore or LiveContainer signs it.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$ROOT/deps/angle/ios/libangle.a" ] || "$ROOT/scripts/angle.sh" >/dev/null
cd "$ROOT/platform/ios"
xcodegen --quiet
if [ "$1" = "--sim" ]; then
    xcodebuild -project Fernstone.xcodeproj -scheme Fernstone -configuration Debug -sdk iphonesimulator \
        -destination 'generic/platform=iOS Simulator' -derivedDataPath "$ROOT/build/ios-dd" ARCHS=arm64 \
        -quiet build
    echo "$ROOT/build/ios-dd/Build/Products/Debug-iphonesimulator/Fernstone.app"
    exit 0
fi
xcodebuild -project Fernstone.xcodeproj -scheme Fernstone -configuration Release \
    -destination 'generic/platform=iOS' -derivedDataPath "$ROOT/build/ios-dd" -quiet build
rm -rf "$ROOT/build/ipa"
mkdir -p "$ROOT/build/ipa/Payload"
cp -R "$ROOT/build/ios-dd/Build/Products/Release-iphoneos/Fernstone.app" "$ROOT/build/ipa/Payload/"
(cd "$ROOT/build/ipa" && zip -qry ../Fernstone.ipa Payload)
echo "$ROOT/build/Fernstone.ipa"
