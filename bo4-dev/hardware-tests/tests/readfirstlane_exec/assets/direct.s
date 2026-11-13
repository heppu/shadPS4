.text
.p2align 8
.long 0xbeeb03ff, 0
v_add_i32 v1, vcc, 256, v0
s_mov_b64 s[8:9], exec
s_mov_b64 exec, s[0:1]
v_readfirstlane_b32 s10, v1
s_mov_b64 exec, s[8:9]
v_mov_b32 v2, s10
v_mov_b32 v3, 0
v_mov_b32 v4, 0
v_mov_b32 v5, 0
v_lshlrev_b32 v6, 2, v0
buffer_store_dwordx4 v[2:5], v6, s[4:7], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
