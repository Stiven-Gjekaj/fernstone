#!/bin/sh
# Build the Cydia package of Fernstone for iOS 6 into build/.
# Usage: scripts/deb.sh
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
"$ROOT/scripts/app.sh" >/dev/null
VERSION=$(plutil -extract CFBundleShortVersionString raw "$ROOT/platform/ios6/Info.plist")
NAME="io.github.stivengjekaj.fernstone_${VERSION}_iphoneos-arm.deb"
APP="$ROOT/build/ios6/Fernstone.app"

# iOS 6 reads only the first code directory, so SHA-1 must be first.
codesign -s - --digest-algorithm=sha1,sha256 -f "$APP/Fernstone" 2>/dev/null

STAGE="$ROOT/build/deb"
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" "$STAGE/Applications"
cp -R "$APP" "$STAGE/Applications/"
sed "s/@VERSION@/$VERSION/" "$ROOT/packaging/control" > "$STAGE/DEBIAN/control"
cp "$ROOT/packaging/postinst" "$ROOT/packaging/postrm" "$STAGE/DEBIAN/"
chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm" "$STAGE/Applications/Fernstone.app/Fernstone"
xattr -cr "$STAGE"

# The dpkg of Cydia on iOS 6 reads gzip, not xz or zstd.
dpkg-deb -Zgzip --root-owner-group -b "$STAGE" "$ROOT/build/$NAME" >/dev/null
echo "$ROOT/build/$NAME"
