set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
python
import gdb
class Layout(gdb.Breakpoint):
 def stop(self):
  try:
   print('COPY_LAYOUT',gdb.parse_and_eval('src->info.size'),gdb.parse_and_eval('src->info.pitch'),gdb.parse_and_eval('src->info.mips_layout[0]'),gdb.parse_and_eval('download_copies._M_ptr[0]'))
  except Exception as e:print('COPY_ERROR',e)
  return False
Layout('Vulkan::Runtime::DownloadImage')
end
run
