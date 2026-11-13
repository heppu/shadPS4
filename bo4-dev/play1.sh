#!/bin/sh
cd /mnt/games/PS4/bo4-dev
(./run 240 >/dev/null 2>&1 &)
sleep 60; ./shot p0
./key -P n -s 150 -p n; sleep 15; ./shot p1
sleep 20; ./shot p2
./key -P n -s 150 -p n; sleep 20; ./shot p3
sleep 40; ./shot p4
sleep 60; ./shot p5
sleep 30
/bin/busybox grep -E 'Critical|Unhandled|Assert|exit=' out/last/stdout.txt | head -5 | cut -c1-300
