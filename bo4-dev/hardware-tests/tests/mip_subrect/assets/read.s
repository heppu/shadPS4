.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v1, 0
v_mov_b32 v2, 0
v_mov_b32 v3, v0
image_get_resinfo v[4:5], v0, s[0:7] dmask:0x3
image_load_mip v8, v[1:4], s[0:7] dmask:0x1 unorm
s_waitcnt vmcnt(0)
buffer_store_dwordx2 v[4:5], v0, s[8:11], 0 idxen glc
buffer_store_dword v8, v0, s[8:11], 0 idxen offset:8 glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
