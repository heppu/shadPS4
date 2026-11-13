from pathlib import Path

target_start = 0x24441dd500
target_end = target_start + 0x180000
for proc in Path('/proc').iterdir():
    if not proc.name.isdigit():
        continue
    try:
        if str((proc / 'exe').readlink()).split(' (deleted)')[0] != '/src/build/shadps4':
            continue
        lines = (proc / 'maps').read_text().splitlines()
    except (OSError, PermissionError):
        continue
    maps = []
    for line in lines:
        parts = line.split(maxsplit=5)
        start, end = (int(x, 16) for x in parts[0].split('-'))
        maps.append((start, end, int(parts[2], 16), parts[3], parts[4], line))
    for start, end, offset, device, inode, line in maps:
        if start >= target_end or end <= target_start:
            continue
        print('TARGET', line)
        if inode == '0':
            continue
        physical_start = offset + max(start, target_start) - start
        physical_end = offset + min(end, target_end) - start
        for other_start, other_end, other_offset, other_device, other_inode, other_line in maps:
            if other_start == start or (other_device, other_inode) != (device, inode):
                continue
            lo = max(physical_start, other_offset)
            hi = min(physical_end, other_offset + other_end - other_start)
            if lo < hi:
                print('ALIAS', hex(other_start + lo - other_offset), hex(hi - lo), other_line)
