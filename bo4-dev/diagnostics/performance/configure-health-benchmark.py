import os, subprocess, sys, time
from pathlib import Path
os.environ.update(DISPLAY=':0', XDG_RUNTIME_DIR='/tmp/rt', WAYLAND_DISPLAY='wayland-1')
label = sys.argv[1]
log = Path('/out/performance/' + label + '-route.log')
deadline = time.monotonic() + 220
while not log.exists() or 'lobby' not in log.read_text():
    if time.monotonic() > deadline:
        raise RuntimeError('Lobby timeout')
    time.sleep(1)
time.sleep(5)
def tap(*keys):
    subprocess.run(['python3', '/out/performance/tap-keys.py', *keys], check=True)
def shot(suffix):
    subprocess.run(['grim', '/out/performance/' + label + '-' + suffix + '.png'], check=True)
tap('Right', 'Up', 'Up', 'n')
time.sleep(3)
tap('n')
time.sleep(3)
tap('n')
time.sleep(3)
tap('n')
time.sleep(2)
shot('rules')
tap('u', 'u', 'u', 'u', 'n', 'Right', 'Right', 'Right', 'Down', 'Left', 'Down', 'Left')
shot('health')
print('health settings prepared; inspect screenshot before starting', flush=True)
