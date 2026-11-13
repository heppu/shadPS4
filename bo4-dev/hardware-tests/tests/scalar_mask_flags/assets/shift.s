.text
.p2align 8
.long 0xbeeb03ff, 0
s_lshl_b64 s[8:9], s[0:1], 0
s_cselect_b32 s10, 1, 0
v_mov_b32 v4, 0
s_cbranch_scc0 zero
v_mov_b32 v4, 1
zero:
v_mov_b32 v1, s8
v_mov_b32 v2, s9
v_mov_b32 v3, s10
v_lshlrev_b32 v5, 2, v0
buffer_store_dwordx4 v[1:4], v5, s[4:7], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
