#!/usr/bin/env bash
# RE24 Session 2: ROM-gated 1:1 parity of the native C 3D-view port
# (src/jaguar/avp_gpu/view3d.c) against the ROM's own GPU program.
#   tools/jagref/view3d_port_parity.sh /path/to/AVP.jag [build-dir] [fuzz-seeds-per-pose]
# Ten scripted poses (Alien, Predator, Marine; standing and moving), each
# compared exactly, plus optional randomized scenes per pose.  Nothing derived
# from the cartridge is written to disk.
set -euo pipefail
rom=${1:?AVP.jag}; root="$(cd "$(dirname "$0")/../.." && pwd)"; build=${2:-$root/build-gcc}; seeds=${3:-0}
[ "$(sha256sum "$rom" | cut -d" " -f1)" = b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b ] || { echo "not the verified retail AVP.jag" >&2; exit 2; }
cc=${CC:-cc}
libs="$build/libavp_gpu_original_native.a $build/libavp_jag_refmachine.a $build/libavp_jaguar_trm.a $build/libavp_jaguar_compat.a $build/libavp_musashi.a -lm"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
$cc -std=c99 -O2 -Wall -Wextra -I"$root/include" "$root/tools/jagref/view3d_port_check.c" "$root/tools/jagref/view3d_snapshot.c" $libs -o "$tmp/check"
fail=0
# field  input(start:len:padmask,...)   (UP=1 DOWN=2 LEFT=4 RIGHT=8)
while read -r field input; do
  [ -n "$field" ] || continue
  if out=$("$tmp/check" "$rom" "$field" "${input:--}" "$seeds"); then st=IDENTICAL; else st=DIFFERENT; fail=1; fi
  printf 'field %-5s input %-36s %s | %s\n' "$field" "${input:--}" "$st" "$(echo "$out" | grep -E '^(blits|captured|fuzzed)' | tr '\n' ' ')"
done <<'POSES'
2199
2260 2100:150:4
2300 2100:180:8
2400 2100:120:1,2220:150:6
2500 2100:300:5
2700 2100:200:1,2300:200:9,2500:150:4
2199 1250:4:8
2400 1250:4:8,2100:150:1,2260:100:4
2600 1250:4:4,2300:6:20000
2800 1250:4:4,2300:6:20000,2500:150:1,2660:100:8
POSES
exit $fail
