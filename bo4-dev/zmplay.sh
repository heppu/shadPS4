#!/bin/sh
cd /mnt/games/PS4/bo4-dev
k() { ./key -P $1 -s 60 -p $1; }
(sleep 95; k n; sleep 45; ./shot y0
 k Right; sleep 4; k Right; sleep 4; ./shot y1
 k n; sleep 45; ./shot y2
 k Right; sleep 4; ./shot y3
 k n; sleep 25; ./shot y4
 k n; sleep 40; ./shot y5
 k n; sleep 45; ./shot y6; sleep 50; ./shot y7; sleep 50; ./shot y8; sleep 45; ./shot y9) >/dev/null 2>&1 &
./bprun ${1:-540}
