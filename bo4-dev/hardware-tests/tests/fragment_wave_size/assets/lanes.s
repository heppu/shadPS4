.text
.p2align 8
.long 0xbeeb03ff, 0
v_mbcnt_lo_u32_b32 v0, -1, 0
v_mbcnt_hi_u32_b32 v0, -1, v0
v_cvt_f32_u32 v0, v0
v_mov_b32 v1, 0.5
v_mov_b32 v2, 0.75
v_mov_b32 v3, 1.0
exp mrt0 v0, v1, v2, v3 done vm
s_endpgm
.p2align 3
