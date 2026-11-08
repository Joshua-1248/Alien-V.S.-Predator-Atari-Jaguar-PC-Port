#!/usr/bin/env bash
# RE24 S27: native C equivalence of the original front-end JagPEG GPU program.
# Static JRISC->C is regenerated LOCALLY from the user's verified ROM; no
# captured cartridge bytes or generated ROM-derived C leave the temp folder.
set -euo pipefail
rom=${1:?usage: jagpeg_frontend_native_parity.sh verified_AVP.jag [build-dir] [fields...]}
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${2:-"$root/build-gcc"}
cc=${CC:-gcc}
want=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
[[ "$(sha256sum "$rom" | cut -d' ' -f1)" == "$want" ]] || { echo "not the verified retail AVP.jag" >&2; exit 2; }
libs=("$build/libavp_jag_refmachine.a" "$build/libavp_jaguar_trm.a"
      "$build/libavp_jaguar_compat.a" "$build/libavp_musashi.a")
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
"$cc" -std=c99 -O2 -Wall -Wextra -Werror -I"$root/include" \
  "$root/tools/jagref/jagpeg_frontend_native_check.c" "${libs[@]}" -lm -o "$tmp/capture"
"$tmp/capture" "$rom" 186 --capture "$tmp/gpu.bin"
python3 "$root/tools/jagref/gpu_recomp.py" "$tmp/gpu.bin" F03000 F03A00 avp_jagpeg_frontend_generated >"$tmp/frontend.c"
# F03000-F039FF is executable program, F03A00 starts the mutable work area.
image_hash=$(python3 - "$tmp/gpu.bin" <<'PY'
import sys
s=open(sys.argv[1],'rb').read(0xA00)
assert len(s)==0xA00
h=14695981039346656037
for b in s:h=((h^b)*1099511628211)&((1<<64)-1)
print(f'0x{h:016x}ULL')
PY
)
"$cc" -std=c99 -O1 -Wall -Wextra -Werror -Wno-unused-label -Wno-implicit-fallthrough \
  -I"$root/include" -DFRONTEND_SOURCE='"'"$tmp/frontend.c"'"' \
  -DFRONTEND_IMAGE_HASH="$image_hash" \
  "$root/tools/jagref/jagpeg_frontend_native_check.c" "${libs[@]}" -lm -o "$tmp/check"
shift "$(( $# >= 2 ? 2 : 1 ))"
fields=("${@:-}")
if (( ${#fields[@]} == 0 )) || [[ -z "${fields[0]}" ]]; then
  fields=(186 215 251 992 1022 1054 1081 1110 1136)
fi
"$tmp/check" "$rom" "${fields[@]}"
