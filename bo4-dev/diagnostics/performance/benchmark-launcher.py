import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

root = Path('/mnt/games/PS4/bo4-dev')
source = Path('/home/heppu/code/github.com/heppu/shadPS4')
label = sys.argv[1]
if not re.fullmatch(r'[A-Za-z0-9_-]+', label):
    raise SystemExit('Invalid label')
out = root / 'out/performance'
frames = out / (label + '-frames.json')
if frames.exists():
    raise SystemExit('Use a fresh benchmark label')

def run(*args, **kwargs):
    return subprocess.run(args, check=True, cwd=root, **kwargs)

def inspect():
    result = subprocess.run(['docker', 'inspect', 'shad-run'], capture_output=True, text=True)
    return json.loads(result.stdout)[0] if result.returncode == 0 else None

previous = inspect()
if previous and previous['State']['Running']:
    raise SystemExit('shad-run is already running')
with (out / (label + '-frame-symbol.log')).open('w') as log:
    run('./sd', 'gdb', '-batch', '/src/build/shadps4', '-x', '/out/performance/frame-symbol.gdb',
        env=dict(os.environ, SD_DOCKER_ARGS='--network none'), stdout=log, stderr=subprocess.STDOUT)
def interrupted(signum, frame):
    raise KeyboardInterrupt('Benchmark interrupted')

signal.signal(signal.SIGTERM, interrupted)
signal.signal(signal.SIGHUP, interrupted)
owned = None
with (out / (label + '-launcher.log')).open('w') as log:
    launcher = subprocess.Popen(['./play-fast', '--headless', '600'], cwd=root,
                                stdout=log, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            state = inspect()
            if state and state['State']['Running'] and (not previous or state['Id'] != previous['Id']):
                owned = state['Id']
                break
            if launcher.poll() is not None:
                raise RuntimeError('Launcher exited before creating its container')
            time.sleep(.25)
        else:
            raise RuntimeError('Container launch timeout')
        (out / (label + '-game-sequence.log')).write_text('')
        run('docker', 'exec', '-d', owned, 'sh', '-c',
            'python3 /out/performance/route.py "perf-$1" prepare > "/out/performance/$1-route.log" 2>&1', 'sh', label)
        run('docker', 'exec', '--user', 'root', '-d', owned, 'sh', '/out/performance/measure-on-gameplay.sh', label)
        run('docker', 'run', '--rm', '-d', '--name', 'bo4-perf-launcher', '--network', 'none',
            '--pid=container:' + owned, '--cap-add', 'PERFMON', '--cap-add', 'SYS_PTRACE',
            '--security-opt', 'seccomp=unconfined', '-v', str(root / 'out') + ':/out',
            '-v', str(source) + ':/src:ro', 'shad-perf', 'sh', '/out/performance/perf-on-gameplay.sh', label)
        run('docker', 'exec', '-d', owned, 'sh', '-c',
            'python3 /out/performance/start-match.py "$1" /out/performance/sequence-running-on-gameplay.py > "/out/performance/$1-game-sequence.log" 2>&1', 'sh', label)
        deadline = time.monotonic() + 480
        while not frames.exists():
            if launcher.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError('Native sample did not complete')
            time.sleep(1)
        if not json.loads(frames.read_text())['complete']:
            raise RuntimeError('Native sample incomplete')
        run('docker', 'run', '--rm', '--network', 'none', '--pid=container:' + owned,
            '--cap-add', 'PERFMON', '--cap-add', 'SYS_PTRACE', '--security-opt', 'seccomp=unconfined',
            '-v', str(root / 'out') + ':/out', '-v', str(source) + ':/src:ro',
            'shad-perf', 'sh', '/out/performance/report-profile.sh', label)
    finally:
        if owned:
            subprocess.run(['docker', 'stop', '-t', '1', owned], stdout=subprocess.DEVNULL)
            subprocess.run(['docker', 'stop', '-t', '1', 'bo4-perf-launcher'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            launcher.terminate()
        launcher.wait(timeout=45)
        if owned:
            shutil.copy2(root / 'out/last/stdout.txt', out / (label + '-stdout.txt'))
            shutil.copy2(root / 'out/play-performance.json', out / (label + '-performance.json'))
print('Launcher benchmark completed; inspect captures and restored policy status.')
