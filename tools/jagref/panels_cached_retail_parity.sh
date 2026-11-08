#!/usr/bin/env bash
# S40: SHA-gated original 68000 panel loader versus isolated native panel
# cache-hit and NONE FILES-loader paths. No ROM or decoded asset export.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${1:?usage: panels_cached_retail_parity.sh VERIFIED_RETAIL_AVP.jag [BUILD_DIR]}"
BUILD="${2:-$ROOT/build-gcc}"
EXPECTED=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
ACTUAL="$(sha256sum "$ROM" | awk '{print $1}')"
if [[ "$ACTUAL" != "$EXPECTED" ]];then
  echo 'Wrong Atari Jaguar AVP retail ROM SHA-256; refusing comparison.' >&2
  exit 2
fi
if [[ ! -x "$BUILD/panels_cached_rom_check" ]];then
  echo "Missing verifier: $BUILD/panels_cached_rom_check" >&2
  exit 2
fi
"$BUILD/panels_cached_rom_check" "$ROM"
echo 'S40: cached/ordinary-NONE original $F1F6 parity PASS (11 cases).'
