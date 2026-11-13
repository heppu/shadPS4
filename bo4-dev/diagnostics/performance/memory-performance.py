import os,time,subprocess,json
from pathlib import Path
rows=[]
for i in range(3):
 for label,binary in [('control','shadps4-before-page-locks'),('candidate','shadps4-region-cache')]:
  log=Path('/out/performance')/f'memory-perf-{label}-{i}.log'
  start=time.monotonic()
  with log.open('w') as f:
   p=subprocess.run(['python3','/mnt/games/PS4/bo4-dev/hardware-tests/scripts/run-test.py','buffer_mapping_lifetimes','--shadps4','/out/performance/'+binary,'--timeout','120','--set','GPU.direct_memory_access_enabled=true','--set','GPU.readback_linear_images_enabled=true'],stdout=f,stderr=subprocess.STDOUT)
  rows.append(dict(label=label,iteration=i,seconds=time.monotonic()-start,returncode=p.returncode))
  print(rows[-1],flush=True)
  Path('/out/performance/memory-performance.json').write_text(json.dumps(rows,indent=2))
