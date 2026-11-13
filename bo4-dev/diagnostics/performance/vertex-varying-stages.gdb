set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV nostop noprint pass
handle SIGUSR1 nostop noprint pass
handle SIGUSR2 nostop noprint pass
python
import gdb,struct,json
from pathlib import Path
records=[]
def read(addr,fmt):
 return struct.unpack(fmt,gdb.selected_inferior().read_memory(addr,struct.calcsize(fmt)))[0]
class Pipeline(gdb.Breakpoint):
 def stop(self):
  extra={}
  for field in ['sw_stage','requires_wave64','uses_lane_id','uses_subgroup_id','uses_group_quad','uses_group_shuffle','uses_group_ballot','uses_shader_clock','has_storage_images','shared_guest_segment.used_types','shared_scratch_segment.used_types']:
   try:extra[field]=str(gdb.parse_and_eval('infos._M_ptr[3]->'+field))
   except Exception as e:extra[field]=str(e)
  for field in ['minSubgroupSize','maxSubgroupSize','requiredSubgroupSizeStages']:
   try:extra[field]=str(gdb.parse_and_eval('instance.vk13_props.'+field))
   except Exception as e:extra[field]=str(e)
  print('VERTEX_INFO',json.dumps(extra))
  info=gdb.parse_and_eval('pipeline_info')
  n=int(info['stageCount']);stages=int(info['pStages'])
  for i in range(n):
   p=stages+i*48
   if read(p+20,'<I') != 1:continue
   chain=read(p+8,'<Q');size=None
   while chain:
    if read(chain,'<I')==1000225001:size=read(chain+16,'<I')
    chain=read(chain+8,'<Q')
   records.append({'stage':'vertex','required_subgroup_size':size,'flags':read(p+16,'<I')})
   open('/out/performance/vertex-varying-stages.json','w').write(json.dumps(records,indent=2))
  return False
source=Path('/src/src/video_core/renderer_vulkan/vk_graphics_pipeline.cpp')
line=next(i for i,text in enumerate(source.read_text().splitlines(),1) if 'device.createGraphicsPipelineUnique' in text)
Pipeline(str(source)+':'+str(line),internal=True)
end
run
