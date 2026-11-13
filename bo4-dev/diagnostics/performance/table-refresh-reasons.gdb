set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
python
import gdb,json
rows=[]
class Capture(gdb.Breakpoint):
 def stop(self):
  row={}
  for k in ['table_index','refresh_all','refresh_static']:
   try: row[k]=int(gdb.parse_and_eval(k))
   except Exception: row[k]=None
  try:
   table=gdb.parse_and_eval('this->image_tables[table_index]')
   for name in ['entries','dynamic_entries']:
    v=table[name]['_M_impl'];row[name]=int(v['_M_finish']-v['_M_start'])
   row['depth']=bool(table['is_depth']);row['read']=bool(table['is_read'])
   row['entries_changed']=bool(table['entries_changed'])
   row['set']=int(table['current_set'])
   if row['set']>=0:
    row['set_generation']=int(gdb.parse_and_eval('this->image_table_sets[image_tables[table_index].current_set].generation'))
   row['generation']=int(gdb.parse_and_eval('this->texture_cache.image_generation._M_i'))
   ranges=gdb.parse_and_eval('this->table_remapped_ranges')
   row['remapped']=str(ranges)[:600]
  except Exception as e:row['error']=str(e)
  rows.append(row)
  if len(rows)>=96:
   open('/out/performance/table-refresh-reasons.json','w').write(json.dumps(rows,indent=2));return True
  return False
Capture('Vulkan::Rasterizer::BuildImageTableSet(unsigned int, Shader::ImageResource const*, bool, bool)')
end
continue
detach
quit
