.text
.p2align 8
.long 0xbeeb03ff, 0
v_lshrrev_b32 v1, 2, v0
v_lshrrev_b32 v2, 1, v0
v_and_b32 v3, 1, v2
v_xor_b32 v2, v2, v0
v_and_b32 v2, 1, v2
v_add_i32 v1, vcc, v1, v3
v_cvt_f32_u32 v1, v1
v_mul_f32 v1, 0x3d000000, v1
v_add_f32 v1, -1.0, v1
v_cvt_f32_u32 v2, v2
v_mul_f32 v2, -0.5, v2
v_mov_b32 v3, 0.5
v_mov_b32 v4, 1.0
exp pos0 v1, v2, v3, v4 done
s_endpgm
.p2align 3
