set pagination off
set confirm off
set print pretty off
set debuginfod enabled off
handle SIGSEGV nostop noprint pass
handle SIGBUS nostop noprint pass
python
import gdb, json
from collections import Counter
counts=Counter()
class Entry(gdb.Breakpoint):
 def stop(self):
  counts['calls']+=1
  try:
   owner=gdb.parse_and_eval('(Vulkan::Rasterizer*)$rdi')
   index=int(gdb.parse_and_eval('$esi'))
   generation=int(owner['table_view_generation'])
   tables=owner['image_tables']['_M_impl']['_M_start']
   table=(tables+index).dereference()
   start=table['entries']['_M_impl']['_M_start']
   end=table['entries']['_M_impl']['_M_finish']
   for i in range(int(end-start)):
    entry=(start+i).dereference()
    counts['entries']+=1
    if not bool(entry['is_dynamic']):
     counts['static_entries']+=1
     if int(entry['resolved']) and int(entry['resolved_generation'])==generation:
      counts['static_cached_generation_matches']+=1
  except Exception as e:
   counts['errors']+=1
   if counts['errors']<=3: print(str(e))
  return counts['calls']>=16
Entry('*_ZN6Vulkan10Rasterizer18BuildImageTableSetEjPKN6Shader13ImageResourceEbb')
end
continue
python
open('/out/performance/table-entry-cache-counts.json','w').write(json.dumps(dict(counts),indent=2))
print(dict(counts))
end
detach
quit
