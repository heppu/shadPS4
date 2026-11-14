from pathlib import Path
import time,signal,json,sys
mode=sys.argv[1] if len(sys.argv)>1 else "high"
assert mode in ("high", "profile_peak")
policy=Path('/gpu-performance-level')
status=Path('/out/performance/gpu-profile-'+mode+'.json')
stop=Path('/out/performance/gpu-profile-'+mode+'.stop')
original=policy.read_text().strip()
state={'original':original,'requested':mode,'start':time.time(),'restored':False}
def finish(*args):
 raise SystemExit(0)
signal.signal(signal.SIGTERM,finish)
signal.signal(signal.SIGINT,finish)
try:
 policy.write_text(mode+'\n')
 state['active']=policy.read_text().strip();status.write_text(json.dumps(state,indent=2))
 deadline=time.monotonic()+600
 while time.monotonic()<deadline and not stop.exists():time.sleep(1)
finally:
 if mode == 'profile_peak' and state.get('active') == mode:
  try:policy.write_text('profile_exit\n')
  except OSError:pass
 policy.write_text(original+'\n')
 state['restored']=policy.read_text().strip()==original;state['end']=time.time()
 status.write_text(json.dumps(state,indent=2))
