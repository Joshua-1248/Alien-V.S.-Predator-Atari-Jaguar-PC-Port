#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${1:?usage: files_gzip_stage_retail_parity.sh VERIFIED_RETAIL_AVP.jag [BUILD_DIR]}"
BUILD="${2:-$ROOT/build-linux-gcc}"
EXPECTED=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
[[ "$(sha256sum "$ROM" | awk '{print $1}')" == "$EXPECTED" ]] || { echo 'Wrong retail ROM SHA-256' >&2; exit 2; }
"$BUILD/files_gzip_stage_rom_check" "$ROM"
echo 'S42 original $BF6E GZIP vector staging PASS (10 authentic records)'
