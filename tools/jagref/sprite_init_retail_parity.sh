#!/usr/bin/env bash
# S49 original-ROM verification of natural sprite-init subroutine calls.
# Verifies the original retail Jaguar cartridge by SHA-256 and refuses all
# other images. Never exports cartridge bytes or derived assets.
set -euo pipefail
rom=${1:?usage: sprite_init_retail_parity.sh /path/to/AVP.jag [build-dir]}
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${2:-"$root/build-linux-gcc"}
expected=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
actual=$(sha256sum "$rom" | cut -d' ' -f1)
[[ "$actual" == "$expected" ]] || { echo 'Refusing unverified original ROM' >&2; exit 2; }
fail=0
for input in - 1250:4:8 1250:4:4,2300:6:20000; do
    echo "== original character selection: $input =="
    "$build/sprite_init_rom_check" "$rom" "$input" || fail=1
done
exit "$fail"
