set pagination off
set confirm off
handle SIGSEGV nostop noprint pass
handle SIGUSR1 nostop noprint pass
python
import gdb,json
samples=[]
class Sample(gdb.Breakpoint):
 def stop(self):
  row={}
  for key,expr in [('index','perm_idx'),('count','program->modules.m_holder.m_size')]:
   try:row[key]=int(gdb.parse_and_eval(expr))
   except Exception as e:row[key]=str(e)
  samples.append(row)
  if len(samples)>=96:
   open('/out/performance/variant-lookup.json','w').write(json.dumps(samples,indent=2));return True
  return False
Sample('/src/src/video_core/renderer_vulkan/vk_pipeline_cache.cpp:772')
end
continue
detach
quit
