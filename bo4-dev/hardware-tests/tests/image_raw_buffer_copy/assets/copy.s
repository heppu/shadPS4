.text
.p2align 8
.long 0xbeeb03ff, 0
s_mov_b32 s8, 256
loop:
buffer_load_dword v1, v0, s[0:3], 0 idxen glc
s_waitcnt vmcnt(0)
buffer_store_dword v1, v0, s[4:7], 0 idxen glc
v_add_i32 v0, vcc, 64, v0
s_sub_u32 s8, s8, 1
s_cmp_lg_u32 s8, 0
s_cbranch_scc1 loop
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
