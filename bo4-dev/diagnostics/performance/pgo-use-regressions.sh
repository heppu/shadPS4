#!/bin/sh
set -u
cd /out
for probe in runtime_image_lifetimes buffer_mapping_lifetimes runtime_image_views runtime_image_updates runtime_image_first_use runtime_image_identity runtime_image_loads runtime_image_queries runtime_compact_images runtime_image_dimensions runtime_depth_tables runtime_shadow_dimensions; do
 python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py "$probe" --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=false --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > "/out/performance/pgo-use-regression-$probe.log" 2>&1
 echo "$probe EXIT=$?"
done
#!/bin/sh
set -u
cd /out
for probe in mip_subrect image_raw_buffer_copy stencil_storage_alias subrect_format_alias depth_sample_attachment; do
 python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py "$probe" --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > "/out/performance/pgo-use-extra-$probe.log" 2>&1
 echo "$probe EXIT=$?"
done

for probe in sampler_shared_page early_depth_dma; do
 python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py "$probe" --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > "/out/performance/pgo-use-$probe.log" 2>&1
 echo "$probe EXIT=$?"
done

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py fragment_wave_size --shadps4 /src/build/shadps4 --timeout 120 --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/pgo-use-fragment_wave_size.log 2>&1
echo "fragment_wave_size EXIT=$?"

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py image_table_shared_page --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=false --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/pgo-use-image_table_shared_page.log 2>&1
echo "image_table_shared_page EXIT=$?"

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py vertex_wave_size --shadps4 /src/build/shadps4 --timeout 120 --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/pgo-use-vertex_wave_size.log 2>&1
echo "vertex_wave_size EXIT=$?"

python3 /mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py array_image_readback --shadps4 /src/build/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true --set GPU.readback_linear_images_enabled=true --set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true --set Vulkan.vkvalidation_sync_enabled=true > /out/performance/pgo-use-array_image_readback.log 2>&1
echo "array_image_readback EXIT=$?"
