#!/bin/sh
set -eu
FIRST=$1
LABEL=$2
while ! grep -q '^done$' "/out/performance/$FIRST-game-sequence.log" 2>/dev/null || ! grep -q 'complete True' "/out/performance/$FIRST-frames.log" 2>/dev/null; do sleep 1; done
runuser -u dev -- python3 /out/performance/sequence-moving-combat.py "$LABEL" 0 > "/out/performance/$LABEL-movement.log" 2>&1 &
MOVEMENT_PID=$!
python3 /out/performance/sample-live.py "$LABEL" 80 > "/out/performance/$LABEL-frames.log" 2>&1
wait "$MOVEMENT_PID"
