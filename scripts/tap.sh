#!/bin/sh
# Send debug commands to the app on the phone at "ssh iphone4s".
# Usage: scripts/tap.sh "COMMAND" ["COMMAND" ...]
# The app reads /var/mobile/Media/Fernstone/cmd.txt about four times a
# second. See fern_app_command in core/app.h for the commands.
set -e

for cmd in "$@"; do
    printf '%s\n' "$cmd"
done | ssh iphone4s 'cat >> /var/mobile/Media/Fernstone/cmd.txt'
