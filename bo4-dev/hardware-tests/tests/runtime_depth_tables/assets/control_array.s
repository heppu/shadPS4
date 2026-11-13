.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v1, 0.5
v_mov_b32 v2, 0.5
v_mov_b32 v3, 0.5
v_and_b32 v4, 1, v0
v_cvt_f32_u32 v4, v4
image_sample_c_lz v5, v[1:4], s[0:7], s[8:11] dmask:0x1 da
s_waitcnt vmcnt(0)
v_lshlrev_b32 v9, 6, s16
v_add_i32 v9, vcc, v9, v0
buffer_store_dword v5, v9, s[12:15], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
