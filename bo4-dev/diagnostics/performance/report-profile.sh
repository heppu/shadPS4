#!/bin/sh
set -eu
LABEL=$1
for kind in game waits; do
 if [ -f "/out/performance/$LABEL-$kind.data" ]; then
  perf report -i "/out/performance/$LABEL-$kind.data" --stdio --comms shadPS4:GpuComm --percentage relative -g none --sort symbol --percent-limit 1 > "/out/performance/$LABEL-$kind-report.txt" 2>&1
 fi
done
