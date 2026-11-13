.text
.p2align 8
.long 0xbeeb03ff, 0
v_mov_b32 v0, 0
v_mov_b32 v1, 0
image_load v0, v[0:1], s[0:7] dmask:1
s_waitcnt vmcnt(0)
v_mov_b32 v1, 0x3e800000
v_mov_b32 v2, 0x3ec00000
v_mov_b32 v3, 0x3f800000
exp mrt0 v0, v1, v2, v3 done vm
s_endpgm
.p2align 3
