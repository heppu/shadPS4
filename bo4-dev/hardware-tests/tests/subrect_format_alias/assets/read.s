.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v1, 0
v_mov_b32 v2, 0
v_mov_b32 v3, 0
image_get_resinfo v[4:5], v2, s[0:7] dmask:0x3
image_load v[6:7], v[0:3], s[0:7] dmask:0x3 unorm
s_waitcnt vmcnt(0)
buffer_store_dwordx4 v[4:7], v0, s[8:11], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
