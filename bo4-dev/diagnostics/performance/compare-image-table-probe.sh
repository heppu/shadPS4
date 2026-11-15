#!/bin/sh
set -eu
for round in 1 2 3 4 5 6; do
 for variant in control candidate; do
  if [ "$variant" = control ]; then binary=/out/performance/shadps4-pgo-refreshed; else binary=/out/performance/shadps4-table-storage-final; fi
  python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py image_table_reuse --shadps4 "$binary" --timeout 120 --set GPU.direct_memory_access_enabled=true --set Vulkan.vkvalidation_enabled=false --set Vulkan.vkvalidation_core_enabled=false --set Vulkan.vkvalidation_sync_enabled=false > "/out/performance/table-storage-fixed-$variant-$round.log" 2>&1
  grep 'HOST_RESULT' "/out/performance/table-storage-fixed-$variant-$round.log"
 done
done
