#!/bin/sh
exec gdb -batch -x /out/performance/array-copy-layout.gdb --args /out/performance/shadps4-retained-entries "$@"
