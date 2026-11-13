import json,re,time
from pathlib import Path

def sample():
    rows={}
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            name=(proc/'comm').read_text().strip()
            for fd in (proc/'fdinfo').iterdir():
                try:
                    data=fd.read_text()
                    if 'drm-driver:\tamdgpu' not in data:
                        continue
                    device=re.search(r'drm-pdev:\s*(\S+)',data).group(1)
                    client=re.search(r'drm-client-id:\s*(\d+)',data).group(1)
                    engines={k:int(v) for k,v in re.findall(r'drm-engine-([\w-]+):\s*(\d+) ns',data)}
                    rows[device+':'+client]={'pid':int(proc.name),'name':name,'engines':engines}
                except (OSError,AttributeError):
                    pass
        except OSError:
            pass
    return rows

before=sample()
started=time.monotonic()
time.sleep(2)
after=sample()
elapsed=time.monotonic()-started
for key,row in after.items():
    if key in before:
        row['percent']={engine:100*(value-before[key]['engines'].get(engine,value))/(elapsed*1e9) for engine,value in row['engines'].items()}
print(json.dumps({'elapsed':elapsed,'clients':after},indent=2))
