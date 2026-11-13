set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV SIGUSR1 SIGUSR2 nostop noprint pass
python
import gdb
from pathlib import Path
class Override(gdb.Breakpoint):
 def stop(self):
  print('INFO_TYPE',str(gdb.parse_and_eval('infos').type))
  gdb.execute('set variable infos._M_ptr[3]->requires_wave64 = false')
  print('FORCED_VERTEX_VARYING')
  return False
source=Path('/src/src/video_core/renderer_vulkan/vk_graphics_pipeline.cpp')
line=next(i for i,text in enumerate(source.read_text().splitlines(),1) if 'for (auto& stage : shader_stages)' in text)
Override(str(source)+':'+str(line),internal=True)
end
run
