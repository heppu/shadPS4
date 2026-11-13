#!/bin/sh
set -eu
cd /mnt/games/PS4/bo4-dev
label=${1:?Provide a unique build label}
jobs=${2:-4}
case "$label" in ''|*[!a-zA-Z0-9_-]*) echo 'Invalid build label' >&2; exit 2;; esac
case "$jobs" in 1|2|3|4|5|6|7|8) ;; *) echo 'Jobs must be 1..8' >&2; exit 2;; esac
SD_DOCKER_ARGS="--network none -d --name bo4-build-$label" ./sd sh -c '
 cmake --build /src/build -j "$2" > "/out/performance/$1-build.log" 2>&1
 result=$?
 echo "BUILD_EXIT=$result" >> "/out/performance/$1-build.log"
 exit "$result"
' sh "$label" "$jobs"
