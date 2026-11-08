#!/usr/bin/env bash
# RE25-S45: hash-gated original 68000 $F34E-$F3D8 overlay tail comparison.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
rom="${1:?usage: panels_overlay_retail_parity.sh VERIFIED_RETAIL_AVP.jag [BUILD_DIR]}"
build="${2:-$root/build-linux-gcc}"
expected=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
if [[ "$(sha256sum "$rom" | awk '{print $1}')" != "$expected" ]]; then
  echo 'Wrong retail AVP ROM SHA-256; cannot compare original overlay routine' >&2
  exit 2
fi
"$build/panels_overlay_rom_check" "$rom"
echo 'S45 original-ROM postload panel overlay register/pixel parity PASS (65 cases)'
