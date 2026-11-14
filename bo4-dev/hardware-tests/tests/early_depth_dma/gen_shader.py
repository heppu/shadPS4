import hashlib
from pathlib import Path
import struct
import sys

code = bytearray(Path(sys.argv[1]).read_bytes())
assert len(code) % 8 == 0
struct.pack_into('<I', code, 4, len(code) // 8 - 1)
digest = hashlib.sha256(code).digest()
footer = b'OrbShdr' + bytes([1])
stage = {'fullscreen': 1, 'color': 0, 'read': 0, 'write': 0, 'kill': 0}[Path(sys.argv[1]).stem]
footer += struct.pack('<I', (len(code) << 8) | (stage << 2))
footer += bytes(4) + digest[:8] + digest[8:12]
words = struct.unpack('<%dI' % ((len(code) + len(footer)) // 4), code + footer)
Path(sys.argv[2]).write_text('static const uint32_t ' + Path(sys.argv[1]).stem + '_shader[] = {\n' +
    ',\n'.join('    0x%08xu' % word for word in words) + '\n};\n')
