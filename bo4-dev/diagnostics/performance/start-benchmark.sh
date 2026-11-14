#!/bin/sh
set -eu
cd /mnt/games/PS4/bo4-dev
LABEL=$1
SEQUENCE=${2:-/out/performance/sequence-arena.py}
SD_DOCKER_ARGS='--network none' ./sd gdb -batch /src/build/shadps4 -x /out/performance/frame-symbol.gdb > out/performance/frame-symbol.log 2>&1
VRAM_MB=${BENCH_VRAM_MB:-6144} GTT_MAX=6000 sh out/performance/guarded-run.sh
: > "out/performance/$LABEL-game-sequence.log"
docker exec -d shad-run sh -c 'python3 /out/performance/route.py "perf-$1" prepare > "/out/performance/$1-route.log" 2>&1' sh "$LABEL"
docker exec --user root -d shad-run sh /out/performance/measure-on-gameplay.sh "$LABEL"
docker run --rm -d --name bo4-perf-auto --network none --pid=container:shad-run --cap-add PERFMON --cap-add SYS_PTRACE --security-opt seccomp=unconfined -v /mnt/games/PS4/bo4-dev/out:/out -v /home/heppu/code/github.com/heppu/shadPS4:/src:ro shad-perf sh /out/performance/perf-on-gameplay.sh "$LABEL"
docker exec -d shad-run sh -c 'python3 /out/performance/start-match.py "$1" "$2" > "/out/performance/$1-game-sequence.log" 2>&1' sh "$LABEL" "$SEQUENCE"
