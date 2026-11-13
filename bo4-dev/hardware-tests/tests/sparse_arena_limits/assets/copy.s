.text
.p2align 8
.long 0xbeeb03ff, 0
buffer_load_dword v1, v0, s[0:3], 0 idxen
s_waitcnt vmcnt(0)
buffer_store_dword v1, v0, s[4:7], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
