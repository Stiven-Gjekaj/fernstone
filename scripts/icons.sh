#!/bin/sh
# Draw the logo, the banner, and the app icons with tools/brand.m.
# The PNG files are in git, so that a build does not need this script.
# Usage: scripts/icons.sh
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT/build/brand" "$ROOT/resources"
clang -fobjc-arc -Wall "$ROOT/tools/brand.m" -framework Foundation -framework CoreGraphics -framework ImageIO \
    -framework UniformTypeIdentifiers -o "$ROOT/build/brand/brand"
"$ROOT/build/brand/brand" "$ROOT/build/brand" >/dev/null
cp "$ROOT/build/brand/logo.png" "$ROOT/build/brand/banner.png" "$ROOT/assets/brand/"
# iOS 6: the gloss is in the art, so iOS adds no gloss (UIPrerenderedIcon).
sips -z 57 57 "$ROOT/build/brand/icon-ios6.png" --out "$ROOT/resources/Icon.png" >/dev/null
cp "$ROOT/build/brand/icon-ios6.png" "$ROOT/resources/Icon@2x.png"
# iOS: one 1024 point icon. The system rounds the corners.
CAT="$ROOT/platform/ios/Assets.xcassets/AppIcon.appiconset"
mkdir -p "$CAT"
cp "$ROOT/build/brand/icon-1024.png" "$CAT/icon-1024.png"
cat >"$CAT/Contents.json" <<JSON
{
  "images" : [ { "filename" : "icon-1024.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" } ],
  "info" : { "author" : "xcode", "version" : 1 }
}
JSON
printf '{ "info" : { "author" : "xcode", "version" : 1 } }\n' >"$ROOT/platform/ios/Assets.xcassets/Contents.json"
# macOS: an icns file from an iconset.
SET="$ROOT/build/brand/Fernstone.iconset"
rm -rf "$SET"
mkdir -p "$SET"
for s in 16 32 128 256 512; do
    sips -z $s $s "$ROOT/build/brand/icon-mac.png" --out "$SET/icon_${s}x${s}.png" >/dev/null
    sips -z $((s * 2)) $((s * 2)) "$ROOT/build/brand/icon-mac.png" --out "$SET/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$SET" -o "$ROOT/resources/Fernstone.icns"
echo "$ROOT/assets/brand"
