.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v1, 0
v_mov_b32 v2, 0
v_mov_b32 v3, 1
v_mov_b32 v4, 0
image_load v5, v[1:4], s[0:7] dmask:0x1 da
s_waitcnt vmcnt(0)
v_lshlrev_b32 v9, 6, s12
v_add_i32 v9, vcc, v9, v0
buffer_store_dword v5, v9, s[8:11], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
