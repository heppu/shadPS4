set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
python
import gdb,json
rows=[]
gpu=next(t for t in gdb.selected_inferior().threads() if t.name=='shadPS4:GpuComm')
class Capture(gdb.Breakpoint):
 def stop(self):
  f=gdb.newest_frame();frames=[]
  for _ in range(12):
   if f is None:break
   frames.append(f.name());f=f.older()
  rows.append({'mutex':int(gdb.parse_and_eval('$rdi')),'frames':frames})
  if len(rows)>=128:
   open('/out/performance/render-locks.json','w').write(json.dumps(rows,indent=2));return True
  return False
bp=Capture('*pthread_mutex_unlock');bp.thread=gpu.global_num
end
continue
detach
quit
