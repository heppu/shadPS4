.text
.p2align 8
.long 0xbeeb03ff, 0
buffer_load_dword v1, v0, s[4:7], 0 idxen
s_mov_b32 s12, 0
s_mov_b32 s13, 0
loop:
s_lshl_b32 s14, s12, 2
s_load_dword s15, s[0:1], s14
s_waitcnt lgkmcnt(0)
s_add_u32 s13, s13, s15
s_add_u32 s12, s12, 1
s_cmp_lt_u32 s12, 8
s_cbranch_scc1 loop
s_waitcnt vmcnt(0)
v_mov_b32 v2, s13
buffer_store_dword v2, v0, s[8:11], 0 idxen glc
v_add_i32 v3, vcc, 64, v0
buffer_store_dword v1, v3, s[8:11], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
