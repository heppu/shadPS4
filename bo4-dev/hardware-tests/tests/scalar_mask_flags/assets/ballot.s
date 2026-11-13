.text
.p2align 8
.long 0xbeeb03ff, 0
v_cmp_gt_u32 vcc, s0, v0
s_and_b64 vcc, vcc, vcc
s_cselect_b32 s10, 1, 0
s_mov_b64 s[8:9], vcc
v_mov_b32 v4, 0
s_cbranch_scc0 zero
v_add_i32 v6, vcc, 1, v0
ds_swizzle_b32 v4, v6 offset:0x041f
s_waitcnt lgkmcnt(0)
zero:
v_mov_b32 v1, s8
v_mov_b32 v2, s9
v_mov_b32 v3, s10
v_lshlrev_b32 v5, 2, v0
buffer_store_dwordx4 v[1:4], v5, s[4:7], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
