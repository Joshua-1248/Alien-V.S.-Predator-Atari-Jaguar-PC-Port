#!/usr/bin/env bash
# RE24-S10: original-ROM vs native-port frontend parity (Linux).
#   tools/jagref/frontend_parity.sh /path/to/AVP.jag [build-dir]
# Runs the original cartridge on the TRM reference machine (jagref) to the
# settled title screen, renders the same screen with the live PC frontend
# renderer, and compares all 320x240 displayed pixels.  Nothing derived from
# the ROM is written outside a temporary directory.
set -euo pipefail
rom=${1:?AVP.jag}; root="$(cd "$(dirname "$0")/../.." && pwd)"; build=${2:-$root/build-gcc}
[ "$(sha256sum "$rom" | cut -d" " -f1)" = b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b ] || { echo "not the verified retail AVP.jag" >&2; exit 2; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
"$build/jagref" "$rom" 1000 200 "$tmp" >/dev/null
cc -std=c99 -O2 -I"$root/include" -c "$root/tools/jagref/frontend_parity_probe.c" -o "$tmp/probe.o"
link=$(sed "s# CMakeFiles/avp_pc.dir/src/platform/pc/pc_main.c.o# $tmp/probe.o#; s#-o avp_pc#-o $tmp/probe#" "$build/CMakeFiles/avp_pc.dir/link.txt")
(cd "$build" && eval "$link")
(cd "$tmp" && ./probe "$rom")
python3 - "$tmp/frame_00600.ppm" "$tmp/m_title.ppm" "$tmp/frame_01000.ppm" "$tmp/m_menu.ppm" <<'PY'
import sys
def ppm(p):
    raw=open(p,'rb').read()
    # tolerant P6 parser
    parts=raw.split(maxsplit=4); w,h=int(parts[1]),int(parts[2]); px=parts[4]
    return w,h,px
def cmp(a,b):
    rw,rh,rp=ppm(a); pw,ph,pp=ppm(b); diff=0
    for y in range(240):
        x=rp[y*rw*3:y*rw*3+960]; z=pp[y*pw*3:y*pw*3+960]
        diff+=sum(1 for i in range(0,960,3) if x[i:i+3]!=z[i:i+3])
    return diff
t=cmp(sys.argv[1],sys.argv[2]); m=cmp(sys.argv[3],sys.argv[4])
print(f"title screen: {t} of 76800 pixels differ (original ROM on TRM machine vs live port renderer)")
print(f"title menu  : {m} of 76800 pixels differ (retail 15/16,13/16,11/16 in-place dim)")
sys.exit(1 if (t or m) else 0)
PY
