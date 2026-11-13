.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v1, 0
v_mov_b32 v2, s8
v_mov_b32 v3, s9
image_store v[2:3], v[0:3], s[0:7] dmask:0x3 unorm glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
