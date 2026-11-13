import os,subprocess,time,sys
from pathlib import Path
os.environ.update(DISPLAY=':0',XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1')
label=sys.argv[1]
log=Path('/out/performance/'+label+'-route.log');end=time.monotonic()+220
while time.monotonic()<end:
 if log.exists() and 'lobby' in log.read_text():break
 time.sleep(1)
else:raise RuntimeError('lobby timeout')
def key(k,hold=.25):
 subprocess.run(['xdotool','keydown',k],check=True);time.sleep(hold);subprocess.run(['xdotool','keyup',k],check=True)
key('Right',.8);time.sleep(3)
b=subprocess.check_output(['grim','-g','1020,640 1x1','-t','ppm','-'])[-3:]
if not (b[0]>180 and b[1]>60 and b[2]<180 and b[0]-b[2]>60):raise RuntimeError('Play not highlighted')
key('n',.8);time.sleep(10);key('n',.8)
subprocess.run(['python3',(sys.argv[2] if len(sys.argv)>2 else '/out/ship-fix/sequence-guarded.py'),'perf-'+label+'-game'],check=True)
