#!/usr/bin/env bash
# RE24-S19: ROM-gated per-level parity using the same native stage driver linked into avp_pc.
#   tools/jagref/gpu_pipeline_native_parity.sh /path/to/AVP.jag [build-dir]
# For each scripted pose: run the retail ROM on jagref to the start of a view
# ($F03000), capture GPU RAM + DRAM, audit 68000 overlay patch descriptors, generate parameterized C
# for every sequencer stage from that capture (tools/jagref/gpu_pipeline_recomp.py), then execute the whole
# view both on the TRM JRISC core and as the recompiled C from one identical
# state.  Fails unless every Blitter command, all GPU registers, all DRAM and
# GPU RAM match.  The generated C is derived from the user's ROM and is kept
# only in a temporary directory.
set -euo pipefail
rom=${1:?AVP.jag}; root="$(cd "$(dirname "$0")/../.." && pwd)"; build=${2:-$root/build-gcc}
[ "$(sha256sum "$rom" | cut -d" " -f1)" = b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b ] || { echo "not the verified retail AVP.jag" >&2; exit 2; }
rom=$(cd "$(dirname "$rom")" && pwd)/$(basename "$rom")
cc=${CC:-cc}
libs="$build/libavp_gpu_original_native.a $build/libavp_jag_refmachine.a $build/libavp_jaguar_trm.a $build/libavp_jaguar_compat.a $build/libavp_musashi.a -lm"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# S22: opt-in, per-pose page-address maps; never persist cartridge DRAM,
# generated C or full GPU program in the chosen report directory.
if [ -n "${GPU_VIEW_MAP_DIR:-}" ]; then
  mkdir -p "$GPU_VIEW_MAP_DIR"
  GPU_VIEW_MAP_DIR=$(cd "$GPU_VIEW_MAP_DIR" && pwd)
fi
$cc -std=c99 -O1 -I"$root/include" "$root/tools/jagref/gpu_pipeline_check.c" $libs -o "$tmp/dump"
fail=0
# field  input(start:len:padmask,...)   (UP=1 DOWN=2 LEFT=4 RIGHT=8)
# 1250:4:8 / 1250:4:4 rotate the character select to Predator / Marine; the
# Marine briefing waits for FIRE (2300:6:20000)
while read -r field input; do
  [ -n "$field" ] || continue
  n=$((${n:-0}+1)); d="$tmp/p$n"; mkdir -p "$d"
  (cd "$d" && DUMP=1 "$tmp/dump" "$rom" "$field" "$input" >/dev/null)
  python3 "$root/tools/jagref/gpu_overlay_descriptors.py" "$d/view_gpu.bin" "$d/view_dram.bin" --json "$d/patch-map.json" > "$d/patch-audit.txt"
  python3 "$root/tools/jagref/gpu_pipeline_recomp.py" "$d/view_gpu.bin" "$d/view_dram.bin" --patch-map "$d/patch-map.json" > "$d/pipeline.c" 2>"$d/stages.txt"
  $cc -std=c99 -O1 -w -I"$root/include" -DAVP_GPU_NATIVE_EXEC -DPIPELINE_SOURCE="\"$d/pipeline.c\"" "$root/tools/jagref/gpu_pipeline_check.c" $libs -o "$d/check"
  if [ -n "${GPU_VIEW_MAP_DIR:-}" ]; then
    if (cd "$d" && GPU_SCENE_ACCESS_CSV="$d/access.csv" GPU_SCENE_PROVENANCE_CSV="$d/provenance.csv" "$d/check" "$rom" "$field" "$input" > "$d/result.txt"); then st=IDENTICAL; else st=DIFFERENT; fail=1; fi
    if [ -s "$d/access.csv" ]; then
      cp "$d/access.csv" "$GPU_VIEW_MAP_DIR/pose_${n}_field_${field}.csv"
      python3 "$root/tools/jagref/gpu_view_access_report.py" "$d/access.csv" > "$GPU_VIEW_MAP_DIR/pose_${n}_field_${field}.txt"
      if [ -s "$d/provenance.csv" ]; then
        cp "$d/provenance.csv" "$GPU_VIEW_MAP_DIR/pose_${n}_field_${field}_cpu.csv"
        python3 "$root/tools/jagref/gpu_view_provenance_report.py" "$d/provenance.csv" > "$GPU_VIEW_MAP_DIR/pose_${n}_field_${field}_cpu.txt"
      else
        echo "CPU provenance report missing for pose $n" >&2;fail=1
      fi
    else
      echo "view access map missing for pose $n; refusing silent partial map" >&2
      fail=1
    fi
  else
    if (cd "$d" && env -u GPU_SCENE_ACCESS_CSV -u GPU_SCENE_PROVENANCE_CSV "$d/check" "$rom" "$field" "$input" > "$d/result.txt"); then st=IDENTICAL; else st=DIFFERENT; fail=1; fi
  fi
  printf 'field %-5s input %-28s %s | %s\n' "$field" "${input:--}" "$st" "$(grep -E '^(view starts|blits|DRAM)' "$d/result.txt" | tr '\n' ' ')"
done <<'POSES'
2199
2260 2100:150:4
2300 2100:180:8
2400 2100:120:1,2220:150:6
2500 2100:300:5
2700 2100:200:1,2300:200:9,2500:150:4
2199 1250:4:8
2400 1250:4:8,2100:150:1,2260:100:4
2600 1250:4:4,2300:6:20000
2800 1250:4:4,2300:6:20000,2500:150:1,2660:100:8
POSES
exit $fail
