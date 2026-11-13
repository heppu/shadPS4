.text
.p2align 8
.long 0xbeeb03ff, 0
s_lshl_b32 s9, s8, 5
s_buffer_load_dwordx8 s[16:23], s[0:3], s9
s_waitcnt lgkmcnt(0)
v_and_b32 v1, 1, v0
image_get_resinfo v[4:7], v1, s[16:23] dmask:0xf da
s_waitcnt vmcnt(0)
v_lshlrev_b32 v9, 6, s8
v_add_i32 v9, vcc, v9, v0
buffer_store_dwordx4 v[4:7], v9, s[4:7], 0 idxen glc
s_waitcnt vmcnt(0)
s_endpgm
.p2align 3
