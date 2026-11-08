#!/usr/bin/env bash
# S38: authenticate private user-owned retail cartridge before original GPU
# versus native FILES/JagPEG RGB, final CRY, and Blitter comparisons.
# No decoded game data is saved to disk or included in public packages.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${1:?usage: files_jpeg_native_bridge_parity.sh VERIFIED_RETAIL_AVP.jag [BUILD_DIR]}"
BUILD="${2:-$ROOT/build-gcc}"
EXPECTED=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
ACTUAL="$(sha256sum "$ROM" | awk '{print $1}')"
if [[ "$ACTUAL" != "$EXPECTED" ]];then
  echo 'Wrong retail Jaguar AVP ROM SHA-256; refusing to run.' >&2
  exit 2
fi
if [[ ! -x "$BUILD/files_jpeg_native_rom_check" ]];then
  echo "Missing executable: $BUILD/files_jpeg_native_rom_check" >&2
  echo 'Configure with -DAVP_BUILD_JAG_REFERENCE=ON, then build this target.' >&2
  exit 2
fi
"$BUILD/files_jpeg_native_rom_check" "$ROM" alien 1640 13 1760
"$BUILD/files_jpeg_native_rom_check" "$ROM" predator 1640 10 1950
"$BUILD/files_jpeg_native_rom_check" "$ROM" marine 1600 40 2150
echo 'S38: 63 of 63 full native JagPEG FILES jobs matched the original reference GPU outputs.'
