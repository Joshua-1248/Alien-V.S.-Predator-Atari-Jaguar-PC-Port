#!/usr/bin/env bash
# RE24-S28: record original 3D GPU entry states ONCE, reuse for fast ROM-gated
# native-C vs original-JRISC tests. Snapshots contain retail-derived RAM bytes:
# PRIVATE TEST FIXTURES; NEVER add them to a source archive or distribute them.
# Usage:
#   view3d_fixture_suite.sh capture /path/to/AVP.jag DIR [build-dir]
#   view3d_fixture_suite.sh replay  /path/to/AVP.jag DIR [build-dir] [fuzz-seeds]
set -euo pipefail
mode=${1:-}; rom=${2:-}; out=${3:-}; root=$(cd "$(dirname "$0")/../.." && pwd)
build=${4:-"$root/build-gcc"}; seeds=${5:-0}
if [[ "$mode" != capture && "$mode" != replay ]] || [[ -z "$rom" || -z "$out" ]]; then
    echo 'usage: view3d_fixture_suite.sh {capture|replay} AVP.jag PRIVATE_DIR [build-dir] [fuzz-seeds]' >&2; exit 2
fi
if [[ $(sha256sum -- "$rom" | awk '{print $1}') != b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b ]]; then
    echo 'retail ROM hash mismatch: refusing to capture/replay' >&2; exit 2
fi
rom=$(realpath "$rom"); out=$(realpath -m "$out")
if [[ "$mode" == capture && -e "$out/manifest.tsv" ]]; then
    echo 'capture directory already has a manifest; refusing to overwrite' >&2; exit 2
fi
if [[ "$mode" == replay && ! -f "$out/manifest.tsv" ]]; then
    echo 'no captured view manifest found' >&2; exit 2
fi
umask 077
if [[ "$mode" == capture ]]; then mkdir -p -- "$out"; chmod 700 -- "$out"; fi
cc=${CC:-cc}
libs=("$build/libavp_gpu_original_native.a" "$build/libavp_jag_refmachine.a"
      "$build/libavp_jaguar_trm.a" "$build/libavp_jaguar_compat.a"
      "$build/libavp_musashi.a")
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
"$cc" -std=c99 -O2 -Wall -Wextra -Werror -I"$root/include" \
    "$root/tools/jagref/view3d_port_check.c" "$root/tools/jagref/view3d_snapshot.c" \
    "${libs[@]}" -lm -o "$tmp/check"
if [[ "$mode" == capture ]]; then
    printf 'label\tfield\tinput\tsha256\n' > "$out/manifest.tsv"
    while IFS=' ' read -r label field input; do
        [[ -n "$label" ]] || continue
        snapshot="$out/$label.avpview"
        [[ ! -e "$snapshot" ]] || { echo "refusing overwrite: $snapshot" >&2;exit 2; }
        echo "Capturing $label at field $field (input $input)"
        "$tmp/check" "$rom" --capture "$snapshot" "$field" "$input" 0 > "$out/$label.log"
        chmod 600 -- "$snapshot"
        hash=$(sha256sum -- "$snapshot" | awk '{print $1}')
        printf '%s\t%s\t%s\t%s\n' "$label" "$field" "$input" "$hash" >> "$out/manifest.tsv"
    done <<'POSES'
alien_neutral 2199 -
alien_turn 2260 2100:150:4
alien_right 2300 2100:180:8
alien_door 2400 2100:120:1,2220:150:6
alien_move 2500 2100:300:5
alien_mixed 2700 2100:200:1,2300:200:9,2500:150:4
predator_neutral 2199 1250:4:8
predator_move 2400 1250:4:8,2100:150:1,2260:100:4
marine_neutral 2600 1250:4:4,2300:6:20000
marine_move 2800 1250:4:4,2300:6:20000,2500:150:1,2660:100:8
POSES
    echo "Capture complete. PRIVATE snapshots in $out; do not redistribute."
else
    failures=0; count=0
    while IFS=$'\t' read -r label field input hash; do
        [[ "$label" != label && -n "$label" ]] || continue
        if [[ ! "$label" =~ ^[a-z0-9_]+$ || ! "$hash" =~ ^[a-f0-9]{64}$ ]]; then
            echo 'invalid snapshot manifest entry' >&2; exit 2
        fi
        snapshot="$out/$label.avpview"
        actual=$(sha256sum -- "$snapshot" | awk '{print $1}')
        if [[ "$actual" != "$hash" ]]; then
            echo "corrupt or changed snapshot: $label" >&2;exit 2
        fi
        if "$tmp/check" "$rom" --replay "$snapshot" "$seeds" > "$out/replay_$label.log"; then
            echo "PASS: $label ($seeds additional fuzz views)"
        else
            echo "FAIL: $label" >&2;failures=$((failures+1))
        fi
        count=$((count+1))
    done < "$out/manifest.tsv"
    [[ $count -gt 0 ]] || { echo 'empty manifest' >&2;exit 2; }
    echo "Replayed $count captured views; failed $failures"
    [[ $failures -eq 0 ]]
fi
