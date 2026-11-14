#!/bin/sh
set -eu
LABEL=$1
WAITED=0
while ! grep -q 'complete True' "/out/performance/$LABEL-frames.log" 2>/dev/null; do
 sleep 1
 WAITED=$((WAITED + 1))
 [ "$WAITED" -lt 900 ] || exit 2
done
python3 - <<'PY'
import os,subprocess
pixel=subprocess.check_output(['grim','-g','170,677 1x1','-t','ppm','-'],env=dict(os.environ,XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1'))[-3:]
if min(pixel)<=180:raise SystemExit('No gameplay HUD after sample')
PY
for pid in $(pgrep -f '^python3 /out/performance/sequence-arena.py '); do kill "$pid"; done
runuser -u dev -- env DISPLAY=:0 xdotool keyup o v
runuser -u dev -- python3 /out/performance/sequence-moving-combat.py "$LABEL-moving" > "/out/performance/$LABEL-moving-movement.log" 2>&1 &
MOVEMENT_PID=$!
python3 /out/performance/sample-live.py "$LABEL-moving" 80 > "/out/performance/$LABEL-moving-frames.log" 2>&1
wait "$MOVEMENT_PID"
