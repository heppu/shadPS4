import os,subprocess,time,sys
os.environ['DISPLAY']=':0'
for k in sys.argv[1:]:
 if k.startswith('wait:'):time.sleep(float(k.split(':')[1]));continue
 subprocess.run(['xdotool','keydown',k],check=True);time.sleep(.35);subprocess.run(['xdotool','keyup',k],check=True);time.sleep(1)
