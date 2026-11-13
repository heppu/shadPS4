import os, subprocess, sys, time
from pathlib import Path
label = sys.argv[1]
root = Path('/out/performance')
log = root / (label + '-game-sequence.log')
deadline = time.monotonic() + 300
while not log.exists() or 'gameplay ' not in log.read_text():
    if time.monotonic() > deadline:
        raise RuntimeError('Gameplay did not start')
    time.sleep(1)
print('Collecting 60 seconds of gameplay for compiler training', flush=True)
for _ in range(60):
    time.sleep(1)
os.environ.update(DISPLAY=':0', XDG_RUNTIME_DIR='/tmp/rt', WAYLAND_DISPLAY='wayland-1')
subprocess.run(['grim', str(root / (label + '-checkpoint.png'))], check=True)
(root / 'pgo-dump.request').touch()
deadline = time.monotonic() + 30
status = root / 'pgo-dump.status'
while not status.exists():
    if time.monotonic() > deadline:
        raise RuntimeError('Profile checkpoint timed out')
    time.sleep(1)
text = status.read_text()
(root / (label + '-checkpoint.status')).write_text(text)
if not text.startswith('0 '):
    raise RuntimeError(text)
print(text, flush=True)
