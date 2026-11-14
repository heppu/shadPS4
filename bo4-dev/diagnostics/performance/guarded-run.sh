#!/bin/sh
set -eu
cd /mnt/games/PS4/bo4-dev
N=$(date +%m%d-%H%M%S)
R=out/runs/$N
mkdir -p "$R"
ln -sfn "runs/$N" out/last
SD_DOCKER_ARGS="-d --name shad-run --network none ${BENCH_CPUSET:+--cpuset-cpus=$BENCH_CPUSET} ${BENCH_RADV_PERFTEST:+-e RADV_PERFTEST=$BENCH_RADV_PERFTEST} -e RUN_LOG=/out/runs/$N/stdout.txt" ./sd env SDL_VIDEODRIVER=x11 /mnt/games/PS4/bo4-dev/headless sh -c 'cd /out && exec timeout -s KILL 1200 /src/build/shadps4 -g /mnt/games/PS4/CUSA11100/eboot.bin -f false > "$RUN_LOG" 2>&1'
