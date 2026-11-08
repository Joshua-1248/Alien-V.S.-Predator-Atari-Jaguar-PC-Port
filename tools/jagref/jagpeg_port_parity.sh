#!/usr/bin/env bash
# RE24 S26: reproduce retail level-JagPEG parity against the native C port.
# Both decoder and converter are tested on the original Alien, Predator,
# and Marine level loads. No cartridge bytes are saved or redistributed.
# usage: tools/jagref/jagpeg_port_parity.sh /path/to/AVP.jag [build-dir]
set -euo pipefail
rom=${1:?path to your retail AVP.jag}; root="$(cd "$(dirname "$0")/../.." && pwd)"
build=${2:-"$root/build-gcc"}; cc=${CC:-gcc}
want=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
actual=$(sha256sum "$rom" | cut -d' ' -f1)
if [[ "$actual" != "$want" ]]; then echo "not the verified retail AVP.jag" >&2; exit 2; fi
libs=("$build/libavp_gpu_original_native.a" "$build/libavp_jag_refmachine.a"
      "$build/libavp_jaguar_trm.a" "$build/libavp_jaguar_compat.a"
      "$build/libavp_musashi.a")
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
"$cc" -std=c99 -O2 -Wall -Wextra -Werror -I"$root/include" \
    "$root/tools/jagref/jagpeg_port_check.c" "${libs[@]}" -lm -o "$tmp/jagpeg_port_check"
# The original level-load's first and last fields are drawn from captured
# original cartridge execution, not from guessed external GPU timing.
for spec in 'alien 1640 13 1760' 'predator 1640 10 1950' 'marine 1600 40 2150'; do
  read -r char start jobs stop <<<"$spec"
  for mode in decode convert; do
    echo "=== $char $mode: $jobs jobs ==="
    "$tmp/jagpeg_port_check" "$rom" "$start" "$jobs" "$stop" "$mode" "$char"
  done
done
