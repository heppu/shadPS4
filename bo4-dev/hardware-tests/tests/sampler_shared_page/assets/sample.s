.text
.p2align 8
.long 0xbeeb03ff, 0
s_lshl_b32 s17, s16, 4
s_buffer_load_dwordx4 s[20:23], s[8:11], s17
s_waitcnt lgkmcnt(0)
v_mov_b32 v1, 0x3fa00000
v_mov_b32 v2, 0.5
image_sample_lz v3, v[1:2], s[0:7], s[20:23] dmask:0x1
s_waitcnt vmcnt(0)
v_lshlrev_b32 v4, 6, s16
v_add_i32 v4, vcc, v4, v0
buffer_store_dword v3, v4, s[12:15], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
