.text
.p2align 8
.long 0xbeeb03ff, 0
v_lshlrev_b32 v1, 1, v0
v_and_b32 v1, 2, v1
v_and_b32 v2, 2, v0
v_cvt_f32_u32 v1, v1
v_cvt_f32_u32 v2, v2
v_mad_f32 v1, v1, 2.0, -1.0
v_mad_f32 v2, v2, 2.0, -1.0
v_mov_b32 v3, 0.5
v_mov_b32 v4, 1.0
exp pos0 v1, v2, v3, v4 done
s_endpgm
.p2align 3
