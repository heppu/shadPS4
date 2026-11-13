import os, subprocess, sys, time
os.environ.update(DISPLAY=':0', XDG_RUNTIME_DIR='/tmp/rt', WAYLAND_DISPLAY='wayland-1')
end = time.monotonic() + 240
while time.monotonic() < end:
    pixel = subprocess.check_output(['grim', '-g', '170,677 1x1', '-t', 'ppm', '-'])[-3:]
    if min(pixel) > 180:
        print('gameplay moving', flush=True)
        os.execvp('python3', ['python3', '/out/performance/sequence-circling.py', sys.argv[1]])
    subprocess.run(['xdotool', 'keydown', 'n'], check=True)
    time.sleep(.4)
    subprocess.run(['xdotool', 'keyup', 'n'], check=True)
    time.sleep(3)
raise RuntimeError('Gameplay HUD timeout')
