.text
.p2align 8
.long 0xbeeb03ff, 0
v_and_b32 v1, s2, v0
v_mov_b32 v2, 0x55555555
s_mov_b32 s11, 0
s_mov_b64 exec, s[0:1]
s_and_b64 s[8:9], exec, exec
s_cbranch_scc0 done
loop:
v_readfirstlane_b32 s10, v1
v_cmp_eq_u32 vcc, s10, v1
s_and_saveexec_b64 s[12:13], vcc
v_mov_b32 v2, s10
s_xor_b64 exec, exec, s[12:13]
s_add_u32 s11, s11, 1
s_cmp_ge_u32 s11, 65
s_cbranch_scc1 done
s_and_b64 s[8:9], exec, exec
s_cbranch_scc1 loop
done:
s_mov_b64 s[8:9], exec
s_mov_b64 exec, -1
v_mov_b32 v3, s11
v_mov_b32 v4, s8
v_mov_b32 v5, s9
v_lshlrev_b32 v6, 2, v0
buffer_store_dwordx4 v[2:5], v6, s[4:7], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
