#!/usr/bin/env bash
# S30: retail-file lookup and uncompressed FILES-dispatch parity.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${1:?usage: files_dispatch_parity.sh RETAIL_AVP.jag [BUILD_DIR]}"
BUILD="${2:-$ROOT/build-gcc}"
EXPECTED=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
ACTUAL="$(sha256sum "$ROM" | awk '{print $1}')"
if [[ "$ACTUAL" != "$EXPECTED" ]]; then
  echo 'Retail AVP ROM SHA-256 mismatch; refusing to compare.' >&2
  exit 2
fi
"$BUILD/files_dispatch_rom_check" "$ROM"
