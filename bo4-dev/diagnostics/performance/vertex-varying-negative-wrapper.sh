#!/bin/sh
exec gdb -batch -x /out/performance/vertex-varying-negative.gdb --args /src/build/shadps4 "$@"
