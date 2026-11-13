import json,subprocess,os,struct
from pathlib import Path
pid=int(subprocess.check_output(['pgrep','-f','^/src/build/shadps4']).splitlines()[0])
symbol=json.loads(Path('/out/performance/frame-symbol.json').read_text())
for line in Path('/proc/%d/maps'%pid).read_text().splitlines():
 fields=line.split()
 if len(fields)>=6 and fields[-1]=='/src/build/shadps4' and int(fields[2],16)==0:
  base=int(fields[0].split('-')[0],16);break
else:raise RuntimeError('executable mapping not found')
fd=os.open('/proc/%d/mem'%pid,os.O_RDONLY)
reference=struct.unpack('<Q',os.pread(fd,8,base+symbol['reference_offset']))[0]
os.close(fd)
Path('/out/performance/frame-address.json').write_text(json.dumps({'pid':pid,'address':reference+symbol['member_offset']}))
