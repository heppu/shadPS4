#!/bin/sh
set -eu
input=${1:?Provide the profiled executable}
output=${2:?Provide a distinct prepared executable}
[ "$input" != "$output" ] || { echo 'Output must preserve input executable' >&2; exit 2; }
llvm-objcopy --remove-section=.sframe --remove-section=.rela.sframe "$input" "$output"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
llvm-objcopy --dump-section ".text=$work/original.text" "$input" "$work/original.elf"
llvm-objcopy --dump-section ".text=$work/prepared.text" "$output" "$work/prepared.elf"
cmp "$work/original.text" "$work/prepared.text"
readelf -WS "$output" | grep -q ' .eh_frame '
sha256sum "$input" "$output"
