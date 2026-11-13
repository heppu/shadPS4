.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v1, 0
image_load v4, v[0:3], s[0:7] dmask:0x1 unorm
s_waitcnt vmcnt(0)
buffer_store_dword v4, v0, s[8:11], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
