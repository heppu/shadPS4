#!/bin/sh
set -eu
label=${1:?Provide the control profile label}
input=${2:?Provide the exact profiled executable}
output=${3:?Provide a distinct output executable}
case "$label" in ''|*[!a-zA-Z0-9_-]*) echo 'Invalid profile label' >&2; exit 2;; esac
[ "$input" != "$output" ] || { echo 'Output must preserve input executable' >&2; exit 2; }
perf2bolt -p "/out/performance/$label-game.data" -o "/out/performance/$label.fdata" "$input" > "/out/performance/$label-perf2bolt.log" 2>&1
llvm-bolt "$input" -o "$output" -data="/out/performance/$label.fdata" -reorder-blocks=ext-tsp -reorder-functions=hfsort -update-debug-sections -dyno-stats > "/out/performance/$label-optimize.log" 2>&1
sha256sum "$input" "$output" "/out/performance/$label.fdata" > "/out/performance/$label-bolt-sha256.txt"
echo BOLT_EXIT=0
