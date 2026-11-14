set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
python
import gdb
class Expansion(gdb.Breakpoint):
 def stop(self):
  print('PROBE_IMAGE_EXPANSION')
  return False
class Table(gdb.Breakpoint):
 def stop(self):
  try:
   i=int(gdb.parse_and_eval('table_index'));t=gdb.parse_and_eval('this->image_tables[table_index]')
   generation=int(gdb.parse_and_eval('this->texture_cache.image_generation._M_i'))
   current=int(t['current_set'])
   previous=int(gdb.parse_and_eval('this->image_table_sets[image_tables[table_index].current_set].generation')) if current>=0 else -1
   print('PROBE_TABLE',i,'generation',generation,'previous',previous,'full',bool(gdb.parse_and_eval('refresh_all')),'static',bool(gdb.parse_and_eval('refresh_static')))
  except Exception as e:print('PROBE_ERROR',e)
  return False
Expansion('VideoCore::TextureCache::ExpandImage')
Table('Vulkan::Rasterizer::BuildImageTableSet(unsigned int, Shader::ImageResource const*, bool, bool)')
end
run
