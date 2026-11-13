#!/bin/sh
# usage: menu.sh [run-seconds]   boot headless and drive to the mode select menu
cd /mnt/games/PS4/bo4-dev
(./run ${1:-600} >/dev/null 2>&1 &)
sleep 60; ./shot m0
./key -P n -s 150 -p n; sleep 20; ./shot m1
./key -P n -s 150 -p n; sleep 25; ./shot m2
/bin/busybox grep -E 'Critical|Unhandled|Assert|exit=' out/last/stdout.txt | head -5 | cut -c1-300
