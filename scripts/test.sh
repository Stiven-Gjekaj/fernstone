#!/bin/sh
# Build and run the tests of the core.
# Usage: scripts/test.sh [--phone] [NAME]
# With no option, the tests run on this Mac.
# With --phone, the same tests run on the iPhone at "ssh iphone4s".
# NAME runs only the tests with NAME in their name.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/tests"
mkdir -p "$OUT"
# The tests use the code of the core, but no renderer and no shell.
# app.c joins the core to the renderer, so the tests leave it out.
CORE=$(ls "$ROOT"/core/*.c "$ROOT"/core/world/*.c "$ROOT"/core/assets/*.c "$ROOT"/core/proto/beta/*.c "$ROOT"/core/render/mesh.c "$ROOT"/core/render/fx.c 2>/dev/null | grep -v "/core/app.c$" || true)
TESTS=$(ls "$ROOT"/tests/*.c)
INC="-I$ROOT/core -I$ROOT/core/assets -I$ROOT/core/world -I$ROOT/core/proto/beta -I$ROOT/core/render -I$ROOT/tests"

if [ "$1" = "--phone" ]; then
    shift
    # shellcheck disable=SC2086
    "$ROOT/build.sh" "$OUT/ftests-armv7" $INC $CORE $TESTS -lz >"$OUT/build.log" 2>&1 ||
        { grep -v 'liblaunch.dylib' "$OUT/build.log" >&2; exit 1; }
    ssh iphone4s 'mkdir -p /tmp/ftests'
    scp -q "$OUT/ftests-armv7" iphone4s:/tmp/ftests/ftests
    ssh iphone4s "cd /tmp/ftests && ldid -S ftests && ./ftests $*"
else
    # shellcheck disable=SC2086
    clang -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Wno-unused-parameter -Werror -g -fsanitize=address,undefined \
        $INC $CORE $TESTS -lz -o "$OUT/ftests"
    "$OUT/ftests" "$@"
fi
