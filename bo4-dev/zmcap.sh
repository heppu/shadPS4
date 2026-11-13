#!/bin/sh
# usage: zmcap.sh [run-seconds]   Zombies route on X11, RenderDoc frame capture at RDOC_AT seconds (user/captures)
cd /mnt/games/PS4/bo4-dev
k() { docker exec -e DISPLAY=:0 shad-run sh -c "xdotool windowfocus \$(xdotool search --name shadPS4 | tail -1); sleep 0.3; xdotool keydown $1; sleep 0.15; xdotool keyup $1"; }
sed -i 's/"renderdoc_enabled": false/"renderdoc_enabled": true/' user/config.json
(sleep 225; docker exec -e DISPLAY=:0 shad-run sh -c 'xdotool windowfocus $(xdotool search --name shadPS4 | tail -1); xdotool mousemove 640 360 click 1'; sleep 5; k Up; sleep 10; k n; sleep 120; ./shot c0
 [ "$MODE" = mp ] || { k Right; sleep 10; k Right; sleep 10; }; ./shot c1
 k n; sleep 120
 k Right; sleep 10
 k n; sleep 60; ./shot c2
 k n; sleep 36; ./shot c3; sleep 80; ./shot c4) >/dev/null 2>&1 &
RDOC_AT=${RDOC_AT:-605} RDOC_SPAN=1 GTT_MAX=${GTT_MAX:-9000} SDL_VIDEODRIVER=x11 ./bprun ${1:-760}
sed -i 's/"renderdoc_enabled": true/"renderdoc_enabled": false/' user/config.json
