.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v0, 3
v_mov_b32 v1, 5
v_mov_b32 v2, 0
v_mov_b32 v3, 0
image_load v4, v[0:3], s[0:7] dmask:0x1 unorm glc
s_waitcnt vmcnt(0)
v_add_i32 v4, vcc, 1, v4
image_store v4, v[0:3], s[0:7] dmask:0x1 unorm glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
