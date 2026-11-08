#!/usr/bin/env bash
# RE25 S48: original sprite-set loader $DB3A in the natural level load vs native,
# for the Alien, Predator and Marine start levels.
# Test only against the authenticated user-supplied cartridge. No ROM is packaged.
set -euo pipefail
rom=${1:?usage: sprite_sets_retail_parity.sh /path/to/AVP.jag [build-dir]}
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${2:-"$root/build-linux-gcc"}
expected=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
actual=$(sha256sum "$rom" | cut -d' ' -f1)
[[ "$actual" == "$expected" ]] || { echo 'Refusing unverified ROM' >&2; exit 2; }
fail=0
for input in - 1250:4:8 1250:4:4,2300:6:20000; do
  echo "== character select input: $input"
  "$build/sprite_sets_rom_check" "$rom" "$input" || fail=1
done
exit $fail
