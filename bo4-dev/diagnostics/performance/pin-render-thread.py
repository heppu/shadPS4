import json,os,sys
from pathlib import Path
requested=os.environ.get('BENCH_RENDER_CPUSET')
if not requested:sys.exit(0)
info=json.loads(Path('/out/performance/frame-address.json').read_text())
task=next(p for p in Path('/proc/%d/task'%info['pid']).iterdir() if (p/'comm').read_text().strip()=='shadPS4:GpuComm')
original=os.sched_getaffinity(int(task.name));selected=set()
for part in requested.split(','):
 limits=[int(n) for n in part.split('-')]
 if len(limits)==1:selected.add(limits[0])
 elif len(limits)==2:selected.update(range(limits[0],limits[1]+1))
 else:raise ValueError('invalid CPU range')
if not selected or not selected<=original:raise ValueError('requested CPUs unavailable')
excluded = set(selected)
adjusted = []
if os.environ.get('BENCH_RENDER_EXCLUSIVE') == '1':
 for cpu in selected:
  for part in Path(f'/sys/devices/system/cpu/cpu{cpu}/topology/thread_siblings_list').read_text().strip().split(','):
   limits = [int(n) for n in part.split('-')]
   excluded.update(range(limits[0], limits[-1] + 1))
 for other in task.parent.iterdir():
  if other == task: continue
  try:
   before = os.sched_getaffinity(int(other.name))
   after = before - excluded
   if after and after != before:
    os.sched_setaffinity(int(other.name), after)
    adjusted.append({'tid': int(other.name), 'original': sorted(before), 'actual': sorted(after)})
  except ProcessLookupError: pass
os.sched_setaffinity(int(task.name),selected)
actual=os.sched_getaffinity(int(task.name))
Path('/out/performance/'+sys.argv[1]+'-affinity.json').write_text(json.dumps({'tid':int(task.name),'original':sorted(original),'requested':sorted(selected),'actual':sorted(actual),'excluded_siblings':sorted(excluded),'other_threads':adjusted},indent=2))
if actual!=selected:raise RuntimeError('affinity did not apply')
