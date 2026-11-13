import os, subprocess, time, sys
from pathlib import Path
os.environ.update(DISPLAY=':0',XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1')
out=Path('/out/ship-fix')/('guarded-'+sys.argv[1]);out.mkdir(exist_ok=True)
def key(k,hold=.25):
 subprocess.run(['xdotool','keydown',k],check=True);time.sleep(hold);subprocess.run(['xdotool','keyup',k],check=True)
def shot(name):subprocess.run(['grim',str(out/(name+'.png'))],check=True)
def orange(x,y):
 if not Path('/tmp/rt/wayland-1').exists():return False
 b=subprocess.check_output(['grim','-g',f'{x},{y} 1x1','-t','ppm','-'])[-3:]
 r,g,b=b
 return r>180 and g>60 and b<180 and r-b>60
def title():return any(orange(x,680) for x in (28,40,52))
def zombies():return any(all(orange(x,y) for y in (300,400,420)) for x in (750,752,754,958,962))
def cards():return zombies() or any(all(orange(x,y) for y in (300,400,420)) for x in (318,532,534,538,740))
def wait_for(f,seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if f():return
  time.sleep(1)
 raise RuntimeError('screen wait timeout')
if len(sys.argv)<3 or sys.argv[2]!='lobby':
 wait_for(title,150);shot('title');print('title',flush=True)
 for _ in range(8):
  key('n',.8);time.sleep(6)
  if not title():break
 wait_for(cards,90);time.sleep(2)
 for _ in range(5):
  if zombies():break
  key('Right');time.sleep(2)
 else:raise RuntimeError('Zombies card not found')
 shot('card');key('n',.8);time.sleep(20);
shot('lobby');print('lobby',flush=True)
if len(sys.argv)>2 and sys.argv[2]=='prepare':sys.exit(0)
key('Right');time.sleep(.6)
if len(sys.argv)>3:
 key('Up');time.sleep(.5);key('n',.8);time.sleep(3);key('Down');time.sleep(.6)
 routes={'blood':['Up','Left'], 'classified':['Up'], 'deadnight':['Left','Left','Left'], 'alpha':['Left']}
 for k in routes[sys.argv[3]]:key(k);time.sleep(.6)
 shot('map');key('n',.4);time.sleep(.6);key('b',.4);time.sleep(3);shot('selected');key('Down');time.sleep(.6)
key('n',.8);time.sleep(10);shot('launch')
key('n',.8);time.sleep(30);shot('intro');key('n',1);print('sequence',flush=True)
active=None
for i in range(90):
 shot(str(i))
 pixel=subprocess.check_output(['grim','-g','170,677 1x1','-t','ppm','-'])[-3:]
 if active is None and min(pixel)>180:active=i;print('gameplay',i,flush=True)
 if active is not None:
  step=i-active
  if step==2:key('w',1)
  if step==4:key('l',.5)
  if step in (6,8,10,12,14):key('o',.8)
  if step==16:key('v',.8)
  if step==18:key('u',.8)
 time.sleep(2)
print('done',flush=True)
