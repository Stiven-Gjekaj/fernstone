#!/bin/sh
# Run a Project Poseidon server (Beta 1.7.3) on this Mac, in offline mode.
# Usage: scripts/server.sh [start | stop | log]
# The server, its world, and its log are in server/. Git does not track it.
# The server listens on port 25565 on all interfaces, so the phones on the
# same Wi-Fi reach it at the IP address of this Mac.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIR="$ROOT/server"
TAG=1.1.12-260725-0600-6a7facd
JAR="poseidon-craftbukkit-$TAG.jar"
mkdir -p "$DIR"
cd "$DIR"

case "${1:-start}" in
start)
    if [ ! -f "$JAR" ]; then
        gh release download "$TAG" -R retromcorg/Project-Poseidon -p "$JAR"
    fi
    if [ ! -f server.properties ]; then
        cat >server.properties <<PROPS
online-mode=false
server-port=25565
level-name=world
level-seed=fernstone
max-players=8
view-distance=10
PROPS
    fi
    if [ -f server.pid ] && kill -0 "$(cat server.pid)" 2>/dev/null; then
        echo "server: it runs already (pid $(cat server.pid))"
        exit 0
    fi
    # Poseidon keeps the spawn protection in bukkit.yml. Turn it off, so that
    # each player can build next to the spawn. The file comes on the first
    # start.
    if [ -f bukkit.yml ]; then
        sed -i '' 's/spawn-radius: [0-9]*/spawn-radius: 0/' bukkit.yml
    fi
    # The console reads commands from a pipe, so that "stop" can reach it.
    rm -f console
    mkfifo console
    (tail -f /dev/null >console &)
    nohup java -Xmx1G -jar "$JAR" nogui <console >server.out 2>&1 &
    echo $! >server.pid
    # Wait for the line that says that the world is ready.
    i=0
    while ! grep -q 'Done (' server.out 2>/dev/null; do
        i=$((i + 1))
        [ $i -lt 120 ] || { tail -20 server.out >&2; exit 1; }
        sleep 1
    done
    echo "server: ready on port 25565 (pid $(cat server.pid))"
    ;;
stop)
    [ -f server.pid ] || exit 0
    echo stop >console
    i=0
    while kill -0 "$(cat server.pid)" 2>/dev/null && [ $i -lt 30 ]; do i=$((i + 1)); sleep 1; done
    kill "$(cat server.pid)" 2>/dev/null || true
    pkill -f "tail -f /dev/null" 2>/dev/null || true
    rm -f server.pid console
    echo "server: stopped"
    ;;
log)
    tail -n 40 server.out
    ;;
esac
