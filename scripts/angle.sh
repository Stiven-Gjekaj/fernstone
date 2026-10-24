#!/bin/sh
# Fetch and build MetalANGLE, the ANGLE fork that draws GLES2 with Metal.
# Usage: scripts/angle.sh
# The result is a static framework for iOS (arm64) and one for macOS
# (arm64) in deps/angle/. The iOS and macOS shells link them.
# Git does not track deps/.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEPS="$ROOT/deps"
SRC="$DEPS/metalangle"
REV=ec925142edeb1da3158fd8710ecc6dc2fb1f1f97
OUT="$DEPS/angle"
mkdir -p "$DEPS" "$OUT"

if [ ! -d "$SRC/.git" ]; then
    git clone -q https://github.com/kakashidinho/metalangle.git "$SRC"
fi
git -C "$SRC" checkout -q "$REV"
sh "$SRC/ios/xcode/fetchDependencies.sh" >"$DEPS/angle-fetch.log" 2>&1

# build SCHEME SDK SETTING
build() {
    xcodebuild build -project "$SRC/ios/xcode/OpenGLES.xcodeproj" -scheme "$1" -sdk "$2" \
        -configuration Release ARCHS=arm64 ONLY_ACTIVE_ARCH=NO "$3" \
        -derivedDataPath "$DEPS/angle-dd-$2" CODE_SIGNING_ALLOWED=NO \
        GCC_TREAT_WARNINGS_AS_ERRORS=NO >"$DEPS/angle-$2.log" 2>&1 ||
        { grep -E 'error:' "$DEPS/angle-$2.log" | head -20 >&2; exit 1; }
}
build MetalANGLE_static iphoneos IPHONEOS_DEPLOYMENT_TARGET=15.0
build MetalANGLE_static_mac macosx MACOSX_DEPLOYMENT_TARGET=12.0

rm -rf "$OUT/ios" "$OUT/macos"
mkdir -p "$OUT/ios" "$OUT/macos"
cp -R "$(find "$DEPS/angle-dd-iphoneos" -name 'MetalANGLE.framework' -path '*Release-iphoneos*' | head -1)" "$OUT/ios/"
cp -R "$(find "$DEPS/angle-dd-macosx" -name 'MetalANGLE.framework' -path '*Release*' | head -1)" "$OUT/macos/"
echo "$OUT"
