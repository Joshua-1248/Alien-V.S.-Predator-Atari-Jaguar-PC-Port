#!/usr/bin/env bash
# S42: authenticate owner-provided Jaguar retail ROM before original 68000 check.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${1:?usage: panels_special_lookup_retail_parity.sh VERIFIED_RETAIL_AVP.jag [BUILD_DIR]}"
BUILD="${2:-$ROOT/build-linux-gcc}"
EXPECTED=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
ACTUAL="$(sha256sum "$ROM" | awk '{print $1}')"
if [[ "$ACTUAL" != "$EXPECTED" ]]; then
  echo 'Wrong AVP retail ROM SHA-256: refusing original-code comparison' >&2
  exit 2
fi
"$BUILD/panels_special_lookup_rom_check" "$ROM"
echo 'S42 special-panel $F2D6 descriptor lookup boundary PASS (13 cases)'
