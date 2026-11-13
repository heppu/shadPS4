.text
.p2align 8
.long 0xbeeb03ff, 0
v_cvt_f32_u32 v1, v0
v_add_f32 v1, 0.5, v1
v_mul_f32 v1, 0x3d000000, v1
v_add_f32 v1, -1.0, v1
v_mov_b32 v2, 0xbe800000
v_mov_b32 v3, 0.5
v_mov_b32 v4, 1.0
exp pos0 v1, v2, v3, v4
exp pos1 v4, off, off, off done
s_endpgm
.p2align 3
