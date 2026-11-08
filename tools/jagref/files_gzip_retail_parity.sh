#!/usr/bin/env bash
# S39 original ROM-only GZIP initialization and FILES #239 output parity.
# No cartridge bytes or decoded resources are emitted.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${1:?usage: files_gzip_retail_parity.sh VERIFIED_RETAIL_AVP.jag [BUILD_DIR]}"
BUILD="${2:-$ROOT/build-gcc}"
EXPECTED=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
ACTUAL="$(sha256sum "$ROM" | awk '{print $1}')"
if [[ "$ACTUAL" != "$EXPECTED" ]];then
  echo 'Refusing to run GZIP parity on unverified retail ROM (SHA-256 mismatch).' >&2
  exit 2
fi
for t in files_gzip_rom_check files_gzip_qconv_rom_check;do
  if [[ ! -x "$BUILD/$t" ]];then
    echo "Missing verifier: $BUILD/$t (configure with -DAVP_BUILD_JAG_REFERENCE=ON)" >&2
    exit 2
  fi
done
"$BUILD/files_gzip_rom_check" "$ROM"
"$BUILD/files_gzip_qconv_rom_check" "$ROM"
echo 'S39: GZIP initialization handoff and 128KiB QCONV output parity passed.'
