import json,os,struct,time,sys,subprocess
from pathlib import Path
label=sys.argv[1];duration=int(sys.argv[2]) if len(sys.argv)>2 else 60
info=json.loads(Path('/out/performance/frame-address.json').read_text())
fd=os.open('/proc/%d/mem'%info['pid'],os.O_RDONLY)
task=next(p for p in Path('/proc/%d/task'%info['pid']).iterdir() if (p/'comm').read_text().strip()=='shadPS4:GpuComm')
hz=os.sysconf('SC_CLK_TCK');rows=[];start=time.monotonic();finished=False
def result():
 elapsed=rows[-1]['seconds']-rows[0]['seconds'] if len(rows)>1 else 0
 return {'complete':finished,'fps':(rows[-1]['frame']-rows[0]['frame'])/elapsed if elapsed else 0,'render_cpu_percent':100*(rows[-1]['render_cpu_seconds']-rows[0]['render_cpu_seconds'])/elapsed if elapsed else 0,'gpu_power_policies':sorted({r['gpu_power_policy'] for r in rows if r.get('gpu_power_policy')}),'samples':rows}
try:
 for i in range(duration+1):
  t=time.monotonic();frame=struct.unpack('<i',os.pread(fd,4,info['address']))[0]
  st=(task/'stat').read_text().rsplit(')',1)[1].split()
  gpu=Path('/sys/class/drm/card1/device/gpu_busy_percent')
  policy=gpu.with_name('power_dpm_force_performance_level')
  rows.append({'seconds':t-start,'unix_time':time.time(),'frame':frame,'render_cpu_seconds':(int(st[11])+int(st[12]))/hz,'render_cpu':int(st[36]),'gpu_busy_percent':int(gpu.read_text()) if gpu.exists() else None,'gpu_power_policy':policy.read_text().strip() if policy.exists() else None,'gpu_clock':next((line.strip() for line in gpu.with_name('pp_dpm_sclk').read_text().splitlines() if '*' in line),None) if gpu.with_name('pp_dpm_sclk').exists() else None})
  if i and i%10==0:
   fps=(frame-rows[-11]['frame'])/(rows[-1]['seconds']-rows[-11]['seconds'])
   print('elapsed',i,'last10_fps',round(fps,2),'cpu',st[36],flush=True)
   subprocess.run(['grim','/out/performance/'+label+f'-sample-{i:03}.png'],env=dict(os.environ,XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1'),check=False)
   Path('/out/performance/'+label+'-frames.partial.json').write_text(json.dumps(result(),indent=2))
  if i<duration:time.sleep(max(0,start+i+1-time.monotonic()))
 finished=True
 subprocess.run(['grim','/out/performance/'+label+'-sample-end.png'],env=dict(os.environ,XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1'),check=False)
finally:
 os.close(fd)
 if rows:Path('/out/performance/'+label+'-frames.json').write_text(json.dumps(result(),indent=2));print('FPS',result()['fps'],'complete',finished,flush=True)
