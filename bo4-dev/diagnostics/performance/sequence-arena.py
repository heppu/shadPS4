import os,subprocess,time,sys
from pathlib import Path
os.environ.update(DISPLAY=':0',XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1')
out=Path('/out/ship-fix')/('guarded-'+sys.argv[1]);out.mkdir(exist_ok=True)
def key(k,hold=.25):
 subprocess.run(['xdotool','keydown',k],check=True);time.sleep(hold);subprocess.run(['xdotool','keyup',k],check=True)
def shot(name):subprocess.run(['grim',str(out/(name+'.png'))],check=True)
time.sleep(30);key('n',1)
active=None
for i in range(120):
 shot(str(i))
 pixel=subprocess.check_output(['grim','-g','170,677 1x1','-t','ppm','-'])[-3:]
 if active is None and min(pixel)>180:active=i;print('gameplay',i,flush=True)
 if active is not None:
  step=i-active
  if step%6==5:key('v',.5)
  else:key('o',.6)
 time.sleep(2)
print('done',flush=True)
