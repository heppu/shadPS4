#!/bin/sh
cd /mnt/games/PS4/bo4-dev
(sleep ${W:-90}; ./key -P n -s 150 -p n; sleep 30; ./key -P n -s 150 -p n; sleep 40; ./shot bm) >/dev/null 2>&1 &
./bprun ${1:-200}
