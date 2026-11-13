#!/bin/sh
set -eu
LABEL=$1
while ! grep -q '^gameplay ' "/out/performance/$LABEL-game-sequence.log"; do sleep 1; done
PID=$(pgrep -f '^/src/build/shadps4')
perf record --proc-map-timeout 30000 -o "/out/performance/$LABEL-game.data" -e cycles:u -F 99 --call-graph dwarf,4096 -p "$PID" -- sleep 20 > "/out/performance/$LABEL-record.log" 2>&1
