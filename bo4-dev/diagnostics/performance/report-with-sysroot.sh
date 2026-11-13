#!/bin/sh
set -eu
LABEL=$1
PID=$(pgrep -f '^/src/build/shadps4')
perf report -i "/out/performance/$LABEL-game.data" --symfs="/proc/$PID/root" --stdio --comms shadPS4:GpuComm --percentage relative -g none --sort symbol --percent-limit 1 > "/out/performance/$LABEL-resolved-report.txt" 2>&1
perf report -i "/out/performance/$LABEL-game.data" --symfs="/proc/$PID/root" --stdio --comms shadPS4:GpuComm --percentage relative --call-graph=callee,0.8,graph --sort symbol --percent-limit 2 > "/out/performance/$LABEL-resolved-stacks.txt" 2>&1
