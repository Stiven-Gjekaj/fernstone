#!/bin/sh
# Build Fernstone.app for iOS 6 (armv7) into build/ios6/.
# Usage: scripts/app.sh [--debug]
# With --debug, the app writes its log to
# /var/mobile/Media/Fernstone/log.txt.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
APP="$ROOT/build/ios6/Fernstone.app"
DEBUG=""
if [ "$1" = "--debug" ]; then DEBUG="-DFERN_DEBUG"; fi
rm -rf "$APP"
mkdir -p "$APP"

# shellcheck disable=SC2046
"$ROOT/build.sh" "$APP/Fernstone" $DEBUG -I"$ROOT/core" -I"$ROOT/core/render" -I"$ROOT/core/world" \
    -I"$ROOT/core/proto/beta" \
    $(ls "$ROOT"/core/*.c "$ROOT"/core/*/*.c "$ROOT"/core/proto/beta/*.c 2>/dev/null) \
    $(ls "$ROOT"/platform/ios6/*.m) \
    -lz -framework Foundation -framework UIKit -framework CoreGraphics -framework QuartzCore \
    -framework OpenGLES \
    2>&1 | grep -v 'liblaunch.dylib' >&2 || true
[ -f "$APP/Fernstone" ]

cp "$ROOT/platform/ios6/Info.plist" "$APP/Info.plist"
cp "$ROOT"/resources/Icon*.png "$APP/" 2>/dev/null || true
printf 'APPL????' > "$APP/PkgInfo"
echo "$APP"
