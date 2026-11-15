#!/bin/sh
set -u
cd /out
failures=0
record() {
 echo "$1 EXIT=$2"
 if [ "$2" -ne 0 ]; then failures=$((failures + 1)); fi
}
for probe in runtime_image_lifetimes buffer_mapping_lifetimes runtime_image_views runtime_image_updates runtime_image_first_use runtime_image_identity runtime_image_loads runtime_image_queries runtime_compact_images runtime_image_dimensions runtime_depth_tables runtime_shadow_dimensions; do
 python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py "$probe" --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=false --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > "/out/performance/region-view-regression-$probe.log" 2>&1
 record "$probe" "$?"
done
for probe in mip_subrect image_raw_buffer_copy stencil_storage_alias subrect_format_alias depth_sample_attachment; do
 python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py "$probe" --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > "/out/performance/region-view-extra-$probe.log" 2>&1
 record "$probe" "$?"
done

for probe in sampler_shared_page early_depth_dma; do
 python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py "$probe" --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > "/out/performance/region-view-$probe.log" 2>&1
 record "$probe" "$?"
done

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py fragment_wave_size --shadps4 /src/build/shadps4 --timeout 120 --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/region-view-fragment_wave_size.log 2>&1
record "fragment_wave_size" "$?"

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py image_table_shared_page --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=false --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/region-view-image_table_shared_page.log 2>&1
record "image_table_shared_page" "$?"

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py vertex_wave_size --shadps4 /src/build/shadps4 --timeout 120 --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/region-view-vertex_wave_size.log 2>&1
record "vertex_wave_size" "$?"

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py array_image_readback --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/region-view-array_image_readback.log 2>&1
record "array_image_readback" "$?"

for probe in buffer_readback_threads wait_reg_mem_coherence; do
 python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py "$probe" --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > "/out/performance/region-view-$probe.log" 2>&1
 record "$probe" "$?"
done

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py eop_image_ordering --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/region-view-eop_image_ordering.log 2>&1
record "eop_image_ordering" "$?"

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py shader_variant_reuse --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/region-view-shader_variant_reuse.log 2>&1
record "shader_variant_reuse" "$?"

echo "PROBES_FAILED=$failures"
[ "$failures" -eq 0 ]
