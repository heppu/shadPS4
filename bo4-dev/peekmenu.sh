#!/bin/sh
cd /mnt/games/PS4/bo4-dev
(sleep 75; ./key -P n -s 150 -p n; sleep 25; ./key -P n -s 150 -p n; sleep 30; ./shot pk) >/dev/null 2>&1 &
./peekrun 150
