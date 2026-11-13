#!/bin/sh
set -eu
for repeat in 1 2 3; do
 for variant in control precise; do
  binary=/out/performance/shadps4-before-overlap
  if [ "$variant" = precise ]; then binary=/src/build/shadps4; fi
  python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py wait_reg_mem_coherence --shadps4 "$binary" --timeout 120 --set GPU.direct_memory_access_enabled=true > "/out/performance/poll-micro-$variant-$repeat.log" 2>&1
  tail -1 "/out/performance/poll-micro-$variant-$repeat.log"
 done
done
