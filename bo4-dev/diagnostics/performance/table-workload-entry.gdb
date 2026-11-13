set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
python
import gdb,json
rows=[]
def length(v):return int(v['_M_impl']['_M_finish']-v['_M_impl']['_M_start'])
class Capture(gdb.Breakpoint):
 def stop(self):
  try:
   t=gdb.parse_and_eval('((Vulkan::Rasterizer*)$rdi)->image_tables._M_impl._M_start[(unsigned int)$esi]' )
   rows.append({'all':bool(gdb.parse_and_eval('$cl')),'static':bool(gdb.parse_and_eval('$r8b')),'entries':length(t['entries']),'dynamic':length(t['dynamic_entries']),'changed':bool(t['entries_changed']),'view_generation':int(gdb.parse_and_eval('((Vulkan::Rasterizer*)$rdi)->table_view_generation')),'validated_generation':int(t['validated_view_generation']),'depth':bool(t['is_depth']),'read':bool(t['is_read'])})
  except Exception as e:rows.append({'error':str(e)})
  if len(rows)>=96:
   open('/out/performance/table-workload.json','w').write(json.dumps(rows,indent=2));return True
  return False
Capture("*'Vulkan::Rasterizer::BuildImageTableSet(unsigned int, Shader::ImageResource const*, bool, bool)'")
end
continue
detach
quit
