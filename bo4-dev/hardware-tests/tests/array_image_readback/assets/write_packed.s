.text
.p2align 8
.long 0xbeeb03ff, 0
v_and_b32 v1, 7, v0
v_lshrrev_b32 v2, 3, v0
v_mov_b32 v3, s12
v_mov_b32 v4, s9
v_mov_b32 v5, s9
v_mov_b32 v6, s9
v_mov_b32 v7, 0
image_store v[4:6], v[1:4], s[0:7] dmask:0x7 unorm glc da
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
