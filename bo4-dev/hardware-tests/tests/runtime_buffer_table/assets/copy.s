.text
.p2align 8
.long 0xbeeb03ff, 0
s_lshl_b32 s4, s4, 5
s_buffer_load_dwordx8 s[8:15], s[0:3], s4
s_waitcnt lgkmcnt(0)
buffer_load_dword v1, v0, s[8:11], 0 idxen
s_waitcnt vmcnt(0)
buffer_store_dword v1, v0, s[12:15], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
