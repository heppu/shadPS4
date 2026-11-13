from pathlib import Path
import os,subprocess,time
log=Path('/out/performance/pgo-checkpoint-game-sequence.log')
while not log.exists() or 'gameplay moving' not in log.read_text():
    time.sleep(1)
print('Training combat started',flush=True)
for elapsed in range(10,71,10):
    time.sleep(10)
    print('Training seconds',elapsed,flush=True)
Path('/out/performance/pgo-dump.request').touch()
status=Path('/out/performance/pgo-dump.status')
for _ in range(60):
    if status.exists():
        print(status.read_text(),flush=True)
        break
    time.sleep(.5)
else:
    raise RuntimeError('Profile checkpoint did not finish')
subprocess.run(['grim','/out/performance/pgo-checkpoint-end.png'],
               env=dict(os.environ,XDG_RUNTIME_DIR='/tmp/rt',WAYLAND_DISPLAY='wayland-1'),check=True)
