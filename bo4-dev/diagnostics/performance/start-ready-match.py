import os, subprocess, sys, time
os.environ.update(DISPLAY=':0', XDG_RUNTIME_DIR='/tmp/rt', WAYLAND_DISPLAY='wayland-1')
pixel = subprocess.check_output(['grim', '-g', '1020,640 1x1', '-t', 'ppm', '-'])[-3:]
if not (pixel[0] > 180 and pixel[1] > 60 and pixel[2] < 180 and pixel[0] - pixel[2] > 60):
    raise RuntimeError('Play is not highlighted')
subprocess.run(['python3', '/out/performance/tap-keys.py', 'n'], check=True)
time.sleep(10)
subprocess.run(['python3', '/out/performance/tap-keys.py', 'n'], check=True)
subprocess.run(['python3', '/out/performance/sequence-running-on-gameplay.py', sys.argv[1]], check=True)
