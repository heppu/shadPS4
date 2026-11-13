set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
python
import gdb,json
rows=[]
class Capture(gdb.Breakpoint):
 def stop(self):
  r={}
  for k in ['device_addr','size','is_write','async','assume_locks']:
   try:r[k]=int(gdb.parse_and_eval(k))
   except Exception:r[k]=None
  try:r['frame']=int(gdb.parse_and_eval('this->liverpool->flip_count'))
  except Exception:r['frame']=None
  r['thread']=gdb.selected_thread().name
  frames=[];f=gdb.newest_frame()
  for i in range(18):
   if not f:break
   try:frames.append({'name':f.name(),'pc':hex(f.pc())});f=f.older()
   except Exception:break
  r['stack']=frames;rows.append(r)
  if len(rows)>=128:
   open('/out/performance/readback-callers.json','w').write(json.dumps(rows,indent=2));return True
  return False
Capture('VideoCore::BufferCache::ReadMemory')
end
continue
detach
quit
