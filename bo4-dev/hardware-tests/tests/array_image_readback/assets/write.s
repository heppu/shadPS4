.text
.p2align 8
.long 0xbeeb03ff, 0
v_and_b32 v1, 7, v0
v_lshrrev_b32 v2, 3, v0
v_mov_b32 v3, s12
v_and_b32 v4, s8, v0
v_lshlrev_b32 v5, 6, s12
v_or_b32 v4, v5, v4
v_xor_b32 v4, s9, v4
image_store v4, v[1:4], s[0:7] dmask:0x1 unorm glc da
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
