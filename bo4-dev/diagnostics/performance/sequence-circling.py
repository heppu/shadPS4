import os,subprocess,time,sys
from pathlib import Path
os.environ.update(DISPLAY=':0',XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1')
def hold(keys,seconds):
 subprocess.run(['xdotool','keydown',*keys],check=True)
 try:time.sleep(seconds)
 finally:subprocess.run(['xdotool','keyup',*keys],check=True)
print('moving',flush=True)
hold(['w','x'],3)
for step in range(30):
 subprocess.run(['xdotool','keydown','w','x'],check=True)
 try:
  hold(['o'],.25);time.sleep(1.25)
  hold(['l'],.3)
  if step%6==5:hold(['v'],.2)
 finally:subprocess.run(['xdotool','keyup','w','x'],check=True)
 print('step',step,flush=True)
print('done',flush=True)
