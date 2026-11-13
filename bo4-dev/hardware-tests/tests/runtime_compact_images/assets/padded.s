.text
.p2align 8
.long 0xbeeb03ff, 0
s_lshl_b32 s13, s16, 5
s_buffer_load_dwordx4 s[20:23], s[0:3], s13
s_waitcnt lgkmcnt(0)
v_mov_b32 v1, 0.5
v_mov_b32 v2, 0.5
image_sample_lz v5, v[1:4], s[20:27], s[4:7] dmask:0x1 r128
s_waitcnt vmcnt(0)
v_lshlrev_b32 v9, 6, s16
v_add_i32 v9, vcc, v9, v0
buffer_store_dword v5, v9, s[8:11], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
