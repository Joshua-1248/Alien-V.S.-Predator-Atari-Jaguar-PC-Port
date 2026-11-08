#!/usr/bin/env bash
# Test only against the authenticated user-supplied cartridge. No ROM is packaged.
set -euo pipefail
rom=${1:?usage: panels_jp80_launch_retail_parity.sh /path/to/AVP.jag [build-dir]}
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${2:-"$root/build-linux-gcc"}
expected=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
actual=$(sha256sum "$rom" | cut -d' ' -f1)
[[ "$actual" == "$expected" ]] || { echo 'Refusing unverified ROM' >&2; exit 2; }
"$build/panels_jp80_launch_rom_check" "$rom"
