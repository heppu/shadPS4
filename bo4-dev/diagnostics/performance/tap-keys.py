import os, subprocess, time, sys
os.environ['DISPLAY'] = ':0'
for key in sys.argv[1:]:
    subprocess.run(['xdotool', 'keydown', key], check=True)
    time.sleep(.25)
    subprocess.run(['xdotool', 'keyup', key], check=True)
    time.sleep(.8)
