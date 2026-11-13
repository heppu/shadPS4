.text
.p2align 8
.long 0xbeeb03ff, 0
s_lshl_b32 s13, s12, 5
s_buffer_load_dwordx8 s[16:23], s[0:3], s13
s_waitcnt lgkmcnt(0)
v_mov_b32 v1, 0.5
v_and_b32 v3, 1, v0
v_cvt_f32_u32 v3, v3
v_mov_b32 v4, 0.25
v_mad_f32 v3, v3, 0.5, v4
v_mov_b32 v2, 0.5
v_mov_b32 v4, 0
image_sample_lz v[5:8], v[1:4], s[16:23], s[4:7] dmask:0xf
s_waitcnt vmcnt(0)
v_lshlrev_b32 v9, 6, s12
v_add_i32 v9, vcc, v9, v0
buffer_store_dword v5, v9, s[8:11], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
