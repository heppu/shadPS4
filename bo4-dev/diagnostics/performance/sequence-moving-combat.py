import os,subprocess,time,sys
from pathlib import Path
os.environ.update(DISPLAY=':0',XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1')
label=sys.argv[1]
out=Path('/out/performance')
def hold(keys,seconds):
 subprocess.run(['xdotool','keydown',*keys],check=True)
 try: time.sleep(seconds)
 finally: subprocess.run(['xdotool','keyup',*keys],check=True)
hold(['w','x'],float(sys.argv[2]) if len(sys.argv)>2 else 4)
subprocess.run(['grim',str(out/(label+'-entered.png'))],check=True)
print('moving',flush=True)
for step in range(24):
 key=['a','s','d','w'][step%4]
 subprocess.run(['xdotool','keydown',key],check=True)
 try:
  for shot in range(4):
   hold(['o'],.35);time.sleep(.25)
  hold(['v'],.4);time.sleep(.6)
 finally: subprocess.run(['xdotool','keyup',key],check=True)
 subprocess.run(['grim',str(out/(label+f'-move-{step:02}.png'))],check=True)
 print('step',step,flush=True)
print('done',flush=True)
