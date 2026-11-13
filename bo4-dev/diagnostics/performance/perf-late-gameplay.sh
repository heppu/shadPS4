#!/bin/sh
set -eu
label=${1:?label required}
delay=${2:-60}
while ! grep -q '^gameplay ' "/out/performance/$label-game-sequence.log"; do sleep 1; done
sleep "$delay"
pid=$(pgrep -f '^/src/build/shadps4')
perf record --proc-map-timeout 30000 -o "/out/performance/$label-late-game.data" -e cycles:u -F 99 --call-graph dwarf,4096 -p "$pid" -- sleep 20 > "/out/performance/$label-late-record.log" 2>&1
