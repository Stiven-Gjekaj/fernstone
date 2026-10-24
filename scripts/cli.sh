#!/bin/sh
# Build the headless client for the Mac into build/fernstone-cli.
# Usage: scripts/cli.sh
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT/build"
CORE=$(ls "$ROOT"/core/*.c "$ROOT"/core/world/*.c "$ROOT"/core/proto/beta/*.c | grep -v "/core/app.c$")
# shellcheck disable=SC2086
clang -std=c99 -D_DEFAULT_SOURCE -O2 -g -Wall -Wextra -Wno-unused-parameter -Werror \
    -I"$ROOT/core" -I"$ROOT/core/world" -I"$ROOT/core/proto/beta" \
    $CORE "$ROOT/platform/cli/main.c" -lz -o "$ROOT/build/fernstone-cli"
echo "$ROOT/build/fernstone-cli"
