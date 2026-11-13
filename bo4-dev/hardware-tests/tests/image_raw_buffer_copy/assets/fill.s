.text
.p2align 8
.long 0xbeeb03ff, 0
s_mov_b32 s9, 0
v_mov_b32 v2, s8
row:
v_mov_b32 v1, s9
image_store v2, v[0:3], s[0:7] dmask:0x1 unorm glc
v_add_i32 v0, vcc, 64, v0
image_store v2, v[0:3], s[0:7] dmask:0x1 unorm glc
v_sub_i32 v0, vcc, v0, 64
s_add_u32 s9, s9, 1
s_cmp_lg_u32 s9, 128
s_cbranch_scc1 row
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
