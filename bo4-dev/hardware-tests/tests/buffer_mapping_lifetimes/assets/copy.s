.text
.p2align 8
.long 0xbeeb03ff, 0
v_lshlrev_b32 v1, 6, s8
v_add_i32 v1, vcc, v1, v0
buffer_load_dword v2, v1, s[0:3], 0 idxen
s_waitcnt vmcnt(0)
v_xor_b32 v2, 0x5a5a1234, v2
buffer_store_dword v2, v1, s[4:7], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
