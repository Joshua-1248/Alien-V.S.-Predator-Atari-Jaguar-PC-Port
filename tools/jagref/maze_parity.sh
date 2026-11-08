#!/usr/bin/env bash
# RE24-S12: original vs port 3D maze view at the original's own camera pose.
#   tools/jagref/maze_parity.sh /path/to/AVP.jag [build-dir]
# Runs the retail ROM on jagref into Alien level gameplay (scripted FIRE),
# reads the camera (x,y,angle at RAM $7FB0, located by matching the LEVELS
# start table), renders the live port renderer at that pose and reports how
# many centre-view pixels agree (|channel diff| <= 8).  Measurement, not a gate:
# the port's raycaster is not yet an exact reconstruction of the GPU renderer.
set -euo pipefail
rom=${1:?AVP.jag}; root="$(cd "$(dirname "$0")/../.." && pwd)"; build=${2:-$root/build-gcc}
[ "$(sha256sum "$rom" | cut -d" " -f1)" = b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b ] || { echo "not the verified retail AVP.jag" >&2; exit 2; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
INPUT="950:6:20000,1150:6:20000,1400:6:20000,1700:6:20000,2000:6:20000" DUMP=7fb0,c,"$tmp/pose.bin" \
  "$build/jagref" "$rom" 2200 2200 "$tmp" >/dev/null
read -r x y a < <(python3 -c "import struct,sys;print(*['%x'%v for v in struct.unpack('>III',open(sys.argv[1],'rb').read())])" "$tmp/pose.bin")
cc -std=c99 -O2 -I"$root/include" -c "$root/tools/jagref/maze_parity_probe.c" -o "$tmp/probe.o"
link=$(sed "s# CMakeFiles/avp_pc.dir/src/platform/pc/pc_main.c.o# $tmp/probe.o#; s#-o avp_pc#-o $tmp/probe#" "$build/CMakeFiles/avp_pc.dir/link.txt")
(cd "$build" && eval "$link")
(cd "$tmp" && ./probe "$rom" 100 "$x" "$y" "$a")
python3 - "$tmp/frame_02200.ppm" "$tmp/maze_port.ppm" "$x" "$y" "$a" <<'PY'
import sys
def ppm(p):
    r=open(p,'rb').read();q=r.split(maxsplit=4);return int(q[1]),int(q[2]),q[4]
aw,ah,ap=ppm(sys.argv[1]);bw,bh,bp=ppm(sys.argv[2])
best=(0,0,0,0)
for dx in range(-2,3):
  for dy in range(-4,5):
    m=t=0
    for y in range(70,190,2):
      for x in range(110,210,2):
        yy,xx=y+dy,x+dx
        if 0<=yy<bh:
          t+=1;i=(y*aw+x)*3;j=(yy*bw+xx)*3
          if max(abs(ap[i+k]-bp[j+k]) for k in range(3))<=8:m+=1
    if m>best[0]:best=(m,t,dx,dy)
print(f"pose x={sys.argv[3]} y={sys.argv[4]} angle={sys.argv[5]}: centre-view agreement {best[0]}/{best[1]} ({100*best[0]/best[1]:.1f}%) at offset dx={best[2]} dy={best[3]}")
PY
