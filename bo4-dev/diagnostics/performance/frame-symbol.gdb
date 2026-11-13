set pagination off
set confirm off
python
import gdb,json,re
reference=int(re.search(r'0x[0-9a-fA-F]+',gdb.execute('info address DebugState',to_string=True)).group(),16)
member=int(gdb.parse_and_eval('&((DebugStateType::DebugStateImpl*)0)->flip_frame_count'))
open('/out/performance/frame-symbol.json','w').write(json.dumps({'reference_offset':reference,'member_offset':member}))
print('frame counter',reference,member)
end
quit
