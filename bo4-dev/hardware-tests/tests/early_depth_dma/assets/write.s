.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v4, 8
buffer_load_dword v1, v4, s[4:7], 0 idxen
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
v_mov_b32 v0, s13
v_mov_b32 v2, 0x3f400000
v_mov_b32 v3, 1.0
v_mov_b32 v4, 0
buffer_store_dword v0, v4, s[8:11], 0 idxen glc
s_waitcnt vmcnt(0)
exp mrt0 v0, v1, v2, v3 done vm
s_endpgm
.p2align 3
