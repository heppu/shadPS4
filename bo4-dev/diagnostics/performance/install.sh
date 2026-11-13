#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TARGET=${1:-/mnt/games/PS4/bo4-dev/out/performance}
mkdir -p "$TARGET"
cp "$HERE"/*.py "$HERE"/*.gdb "$HERE"/*.sh "$TARGET"/
