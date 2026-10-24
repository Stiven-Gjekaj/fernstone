#!/bin/sh
# Build Fernstone.app for iOS 6, copy it to the phone at "ssh iphone4s",
# and open it.
# Usage: scripts/install.sh [--debug]
# FERN_JAR sets the path of the b1.7.3 client jar on this Mac. The script
# copies it to the phone when the phone does not have it.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=/var/mobile/Media/Fernstone
"$ROOT/scripts/app.sh" "$@" >/dev/null

ssh iphone4s "mkdir -p $DEST/jars"
if [ -n "$FERN_JAR" ] && ! ssh iphone4s "test -e $DEST/jars/b1.7.3.jar"; then
    scp -q "$FERN_JAR" "iphone4s:$DEST/jars/b1.7.3.jar"
fi
ssh iphone4s "chown -R mobile:mobile $DEST"

ssh iphone4s 'killall Fernstone >/dev/null 2>&1; rm -rf /Applications/Fernstone.app'
# The sftp server of the phone cannot make a new directory, so copy with tar.
# COPYFILE_DISABLE stops macOS tar from adding "._" files.
COPYFILE_DISABLE=1 tar --no-xattrs -C "$ROOT/build/ios6" -cf - Fernstone.app | ssh iphone4s 'tar -C /Applications -xf -'
# uicache must run as the mobile user, so that SpringBoard sees the new icon.
ssh iphone4s 'cd /Applications/Fernstone.app && ldid -S Fernstone &&
    chown -R root:admin /Applications/Fernstone.app &&
    chmod 755 Fernstone &&
    su mobile -c uicache &&
    open io.github.stivengjekaj.fernstone'
