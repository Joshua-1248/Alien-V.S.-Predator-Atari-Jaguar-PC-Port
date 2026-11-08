#!/usr/bin/env bash
# RE24 S29: private, ROM-gated JagPEG GPU decoder/converter capture and replay.
# Captures original GPU-entry DRAM state once; repeated checks avoid boot/menu.
# PRIVATE captures contain commercial game assets. NEVER redistribute them.
# Usage: jagpeg_fixture_suite.sh capture|replay AVP.jag PRIVATE_DIR [build-dir]
set -euo pipefail
mode=${1:-}; rom=${2:-}; out=${3:-}; root=$(cd "$(dirname "$0")/../.." && pwd)
build=${4:-"$root/build-gcc"}
if [[ "$mode" != capture && "$mode" != replay || -z "$rom" || -z "$out" ]]; then
    echo 'usage: jagpeg_fixture_suite.sh {capture|replay} ROM PRIVATE_DIR [build-dir]' >&2; exit 2
fi
want=b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b
if [[ $(sha256sum -- "$rom" | awk '{print $1}') != "$want" ]]; then
    echo 'refusing unknown retail cartridge' >&2;exit 2
fi
rom=$(realpath "$rom"); out=$(realpath -m "$out"); umask 077
if [[ "$mode" == capture ]]; then
    [[ ! -e "$out/manifest.tsv" ]] || { echo 'refusing to overwrite a captured set' >&2;exit 2; }
    mkdir -p -- "$out";chmod 700 -- "$out"
else
    [[ -s "$out/manifest.tsv" ]] || { echo 'missing fixture manifest' >&2;exit 2; }
fi
cc=${CC:-gcc}; tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
libs=("$build/libavp_gpu_original_native.a" "$build/libavp_jag_refmachine.a"
      "$build/libavp_jaguar_trm.a" "$build/libavp_jaguar_compat.a" "$build/libavp_musashi.a")
"$cc" -std=c99 -O2 -Wall -Wextra -Werror -I"$root/include" \
    "$root/tools/jagref/jagpeg_port_check.c" "$root/tools/jagref/view3d_snapshot.c" \
    "${libs[@]}" -lm -o "$tmp/check"
# These ranges are from real retail level loading, not synthetic timing.
specs=('alien 1640 13 1760' 'predator 1640 10 1950' 'marine 1600 40 2150')
if [[ "$mode" == capture ]]; then
    printf 'character\tmode\tjob\tsha256\n' > "$out/manifest.tsv"
    for spec in "${specs[@]}"; do
        read -r character start jobs stop <<<"$spec"
        for kind in decode convert; do
            echo "Capturing $character/$kind ($jobs original jobs)"
            "$tmp/check" "$rom" --capture "$out" "$start" "$jobs" "$stop" "$kind" "$character" >"$out/capture_${character}_${kind}.log"
            for ((i=1;i<=jobs;i++)); do
                printf -v num '%03d' "$i"
                name="${character}_${kind}_${num}.avpjpeg"
                [[ -f "$out/$name" ]] || { echo "missing $name" >&2;exit 1; }
                hash=$(sha256sum -- "$out/$name" | awk '{print $1}')
                printf '%s\t%s\t%s\t%s\n' "$character" "$kind" "$num" "$hash" >> "$out/manifest.tsv"
            done
        done
    done
    echo '126 private job snapshots captured; KEEP PRIVATE.'
else
    count=0
    while IFS=$'\t' read -r character kind job hash; do
        [[ "$character" != character ]] || continue
        [[ "$character" =~ ^(alien|predator|marine)$ && "$kind" =~ ^(decode|convert)$ &&
           "$job" =~ ^[0-9]{3}$ && "$hash" =~ ^[0-9a-f]{64}$ ]] || { echo 'invalid manifest entry' >&2;exit 2; }
        name="${character}_${kind}_${job}.avpjpeg"
        [[ -f "$out/$name" ]] || { echo "missing $name" >&2;exit 2; }
        [[ $(sha256sum -- "$out/$name" | awk '{print $1}') == "$hash" ]] || { echo "corrupt $name" >&2;exit 2; }
        count=$((count+1))
    done < "$out/manifest.tsv"
    [[ "$count" -eq 126 ]] || { echo "wrong job count: $count (expected 126)" >&2;exit 2; }
    for spec in "${specs[@]}"; do
        read -r character start jobs stop <<<"$spec"
        for kind in decode convert; do
            echo "Replaying $character/$kind ($jobs snapshots)"
            "$tmp/check" "$rom" --replay "$out" "$start" "$jobs" "$stop" "$kind" "$character" > "$out/replay_${character}_${kind}.log"
        done
    done
    echo '126 JagPEG decoder/converter GPU jobs: parity PASS'
fi
