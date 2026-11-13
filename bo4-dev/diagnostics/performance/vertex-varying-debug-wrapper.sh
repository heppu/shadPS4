#!/bin/sh
exec gdb -batch -x /out/performance/vertex-varying-stages.gdb --args /src/build/shadps4 "$@"
