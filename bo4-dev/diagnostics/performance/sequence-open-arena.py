import os, subprocess, sys, time
from pathlib import Path
os.environ.update(DISPLAY=':0', XDG_RUNTIME_DIR='/tmp/rt', WAYLAND_DISPLAY='wayland-1')
label = sys.argv[1]
out = Path('/out/performance')
def hold(keys, seconds):
    subprocess.run(['xdotool', 'keydown', *keys], check=True)
    try:
        time.sleep(seconds)
    finally:
        subprocess.run(['xdotool', 'keyup', *keys], check=True)
hold(['w', 'x'], 5)
hold(['l'], .9)
print('running', flush=True)
for step in range(int(sys.argv[3]) if len(sys.argv) > 3 else 24):
    hold(['w', 'x'], 2)
    hold(['l'], .6)
    hold(['w', 'o'], .35)
    if step % 4 == 3:
        hold(['w', 'v'], .3)
    subprocess.run(['grim', str(out / (label + f'-run-{step:02}.png'))], check=True)
    print('step', step, flush=True)
print('done', flush=True)
