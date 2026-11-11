#!/bin/sh
# usage: zmslow.sh [run-seconds]   Zombies start sequence with stretched timings for slow debug runs
cd /mnt/games/PS4/bo4-dev
k() { ./key -P $1 -s 150 -p $1; }
(sleep 240; ./shot v0; k n; sleep 120; ./shot v1
 k Right; sleep 10; k Right; sleep 10; ./shot v2
 k n; sleep 120; ./shot v3
 k Right; sleep 10; ./shot v4
 k n; sleep 60; ./shot v5
 k n; sleep 30; ./shot w0; sleep 12; ./shot w1; sleep 12; ./shot w2; sleep 12; ./shot w3; sleep 12; ./shot w4; sleep 12; ./shot v6
 k n; sleep 120; ./shot v7; sleep 120; ./shot v8; sleep 120; ./shot v9) >/dev/null 2>&1 &
./bprun ${1:-1150}
