from pathlib import Path
import os, struct

targets = [0x24441dd500, 0x244481d700, 0x2444f58940]
for proc in Path('/proc').iterdir():
    if not proc.name.isdigit():
        continue
    try:
        if str((proc / 'exe').readlink()).split(' (deleted)')[0] != '/src/build/shadps4':
            continue
        lines = (proc / 'maps').read_text().splitlines()
        fd = os.open(proc / 'mem', os.O_RDONLY)
    except OSError:
        continue
    for line in lines:
        fields = line.split()
        lo, hi = [int(x, 16) for x in fields[0].split('-')]
        if not fields[1].startswith('rw') or lo >= 0x20000000:
            continue
        for start in range(lo, min(hi, 0x20000000), 0x1000000):
            data = os.pread(fd, min(0x1000000, hi - start), start)
            for target in targets:
                needle = struct.pack('<Q', target)
                pos = data.find(needle)
                while pos >= 0:
                    address = start + pos
                    context = os.pread(fd, 64, max(lo, address - 16))
                    print(hex(target), 'at', hex(address), context.hex(' '))
                    pos = data.find(needle, pos + 1)
    os.close(fd)
