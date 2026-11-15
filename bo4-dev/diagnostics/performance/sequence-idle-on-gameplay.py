import os,subprocess,time,sys
from pathlib import Path
os.environ.update(DISPLAY=':0',XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1')
deadline=time.monotonic()+240
while time.monotonic()<deadline:
    pixel=subprocess.check_output(['grim','-g','170,677 1x1','-t','ppm','-'])[-3:]
    if min(pixel)>180:
        print('gameplay idle',flush=True)
        break
    subprocess.run(['xdotool','keydown','n'],check=True)
    time.sleep(.4)
    subprocess.run(['xdotool','keyup','n'],check=True)
    time.sleep(3)
else:
    raise RuntimeError('Gameplay HUD timeout')
for second in range(10,121,10):
    time.sleep(10)
    subprocess.run(['grim',str(Path('/out/performance')/(sys.argv[1]+f'-idle-{second:03}.png'))],check=True)
    print('idle',second,flush=True)
