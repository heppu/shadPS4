#!/bin/sh
cd /mnt/games/PS4/bo4-dev
(sleep 95; ./key -P n -s 150 -p n; sleep 30; ./key -P n -s 150 -p n; sleep 40; ./shot z0
 ./key -P Right -s 150 -p Right; sleep 2; ./key -P Right -s 150 -p Right; sleep 3; ./shot z1
 ./key -P n -s 150 -p n; sleep 25; ./shot z2; sleep 30; ./shot z3) >/dev/null 2>&1 &
./bprun ${1:-290}
