#!/bin/sh
# usage: zmslow.sh [run-seconds]   Zombies start sequence with stretched timings for slow debug runs
cd /mnt/games/PS4/bo4-dev; rm -f out/w?.png out/v?.png
k() { ./key -P $1 -s ${2:-150} -p $1; }
# px X Y -> rrggbb of one pixel of the headless session
px() { docker exec -e XDG_RUNTIME_DIR=/tmp/rt -e WAYLAND_DISPLAY=wayland-1 shad-run sh -c "grim -g '$1,$2 1x1' -t ppm - | tail -c 3 | od -An -tx1 | tr -d ' \n'"; }
# orange X Y -> true when the pixel is the orange highlight frame of a selected card
orange() { c=$(px $1 $2); r=$((0x$(echo $c | cut -c1-2))); g=$((0x$(echo $c | cut -c3-4))); b=$((0x$(echo $c | cut -c5-6))); [ $r -gt 180 ] && [ $g -gt 100 ] && [ $b -lt 180 ] && [ $((r-b)) -gt 60 ]; }
# Move right until the Zombies card (third) is selected; a press can be dropped or doubled.
tozombies() {
  for i in 1 2 3 4 5 6; do
    if orange 754 300 || orange 958 300; then echo "route: zombies selected after $i checks"; return 0; fi
    if orange 318 300 || orange 532 300 || orange 534 300; then k Right; else echo "route: no card highlight ($(px 318 300) $(px 534 300) $(px 754 300))"; fi
    sleep 6
  done
  return 1
}
(sleep 240; ./shot v0; k n 60; sleep 120; ./shot v1
 [ "$MODE" = mp ] || tozombies > out/route.log 2>&1; ./shot v2
 k n 60; sleep 120; ./shot v3
 k Right; sleep 10; ./shot v4
 k n 60; sleep 60; ./shot v5
 k n 60; sleep 30; ./shot w0; sleep 12; ./shot w1; sleep 12; ./shot w2; sleep 12; ./shot w3; sleep 12; ./shot w4; sleep 12; ./shot v6
 sleep 120; ./shot v7; sleep 120; ./shot v8; sleep 120; ./shot v9) >/dev/null 2>&1 &
./bprun ${1:-1150}
