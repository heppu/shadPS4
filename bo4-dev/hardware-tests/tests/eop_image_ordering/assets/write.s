.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v1, 0
v_add_i32 v2, vcc, s8, v0
image_store v2, v[0:3], s[0:7] dmask:0x1 unorm glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
