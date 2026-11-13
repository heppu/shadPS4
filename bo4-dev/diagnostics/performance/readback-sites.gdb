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
  for key,expr in [('address','device_addr'),('size','size'),('async','async'),('frame','this->liverpool->flip_count'),('queued','this->liverpool->num_commands._M_i'),('submits','this->liverpool->num_submits._M_i'),('arena','arena_base'),('copy_count','copies.m_holder.m_size')]:
   try:r[key]=int(gdb.parse_and_eval(expr))
   except Exception as e:r[key]=str(e)
  r['stack']=gdb.execute('bt 10',to_string=True)
  rows.append(r)
  if len(rows)>=24:
   open('/out/performance/readback-sites.json','w').write(json.dumps(rows,indent=2));return True
  return False
Capture('/src/src/video_core/buffer_cache/buffer_cache.cpp:245')
end
continue
detach
quit
