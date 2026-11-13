#!/bin/sh
set -eu
LABEL=$1
while ! grep -q '^gameplay ' "/out/performance/$LABEL-game-sequence.log"; do sleep 1; done
python3 /out/performance/locate-frame-counter.py
python3 /out/performance/pin-render-thread.py "$LABEL"
python3 /out/performance/sample-live.py "$LABEL" 180 > "/out/performance/$LABEL-frames.log" 2>&1
