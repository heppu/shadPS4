#!/bin/sh
exec gdb -batch -x /out/performance/image-lifetimes-debug.gdb --args /src/build/shadps4 "$@"
