import hashlib
from pathlib import Path
import struct
import sys

code = bytearray(Path(sys.argv[1]).read_bytes())
assert len(code) % 8 == 0
struct.pack_into('<I', code, 4, len(code) // 8 - 1)
digest = hashlib.sha256(code).digest()
footer = b'OrbShdr' + bytes([1])
footer += struct.pack('<I', (len(code) << 8) | (5 << 2))
footer += bytes(4) + digest[:8] + digest[8:12]
words = struct.unpack('<%dI' % ((len(code) + len(footer)) // 4), code + footer)
Path(sys.argv[2]).write_text('#include <stdint.h>\nstatic const uint32_t '+Path(sys.argv[2]).stem+'_shader[] = {\n' +
    ',\n'.join('    0x%08xu' % word for word in words) + '\n};\n')
