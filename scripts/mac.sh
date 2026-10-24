#!/bin/sh
# Build Fernstone.app for macOS (arm64) with ANGLE into build/macos/.
# Usage: scripts/mac.sh
# The ANGLE library of macOS also holds a CGL backend that the app does not
# use. It needs the OpenGL framework only to link.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$ROOT/deps/angle/macos/libangle.a" ] || "$ROOT/scripts/angle.sh" >/dev/null
APP="$ROOT/build/macos/Fernstone.app"
OBJ="$ROOT/build/macos/obj"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$OBJ"
INC="-I$ROOT/core -I$ROOT/core/world -I$ROOT/core/render -I$ROOT/core/assets -I$ROOT/core/proto/beta -I$ROOT/deps/angle/include"
FLAGS="-arch arm64 -mmacosx-version-min=12.0 -O2 -g -DFERN_ANGLE -DGL_GLEXT_PROTOTYPES -Wall -Wno-unused-parameter"
OBJS=""
for src in "$ROOT"/core/*.c "$ROOT"/core/*/*.c "$ROOT"/core/proto/beta/*.c; do
    o="$OBJ/$(echo "${src#$ROOT/}" | tr / _).o"
    # shellcheck disable=SC2086
    clang -std=c99 -D_DEFAULT_SOURCE $FLAGS $INC -c "$src" -o "$o"
    OBJS="$OBJS $o"
done
# shellcheck disable=SC2086
clang -fobjc-arc $FLAGS $INC -c "$ROOT/platform/macos/main.m" -o "$OBJ/main.o"
# shellcheck disable=SC2086
clang++ -arch arm64 -mmacosx-version-min=12.0 $OBJS "$OBJ/main.o" -L"$ROOT/deps/angle/macos" -langle -lz \
    -framework AppKit -framework QuartzCore -framework Metal -framework IOSurface -framework CoreGraphics \
    -framework IOKit -framework OpenGL -o "$APP/Contents/MacOS/Fernstone" 2>&1 | grep -v "skipping debug map" >&2 || true
[ -f "$APP/Contents/MacOS/Fernstone" ]
cp "$ROOT/platform/macos/Info.plist" "$APP/Contents/Info.plist"
[ -f "$ROOT/resources/Fernstone.icns" ] && cp "$ROOT/resources/Fernstone.icns" "$APP/Contents/Resources/"
codesign -s - -f "$APP" 2>/dev/null
echo "$APP"
