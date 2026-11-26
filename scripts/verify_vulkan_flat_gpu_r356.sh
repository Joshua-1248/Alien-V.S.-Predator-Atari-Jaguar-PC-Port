#!/usr/bin/env bash
# RE47/R356: optional real source-derived flat + wall GPU render pass.
# The Software game continues presenting normally; Hardware remains inactive.
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:---compile-only}"
if [[ "$mode" != "--compile-only" && "$mode" != "--gpu" ]]; then
    echo 'Usage: scripts/verify_vulkan_flat_gpu_r356.sh [--compile-only|--gpu [GAME_ARGS...]]' >&2
    exit 2
fi
bash "$source_dir/scripts/verify_vulkan_flat_gpu_r355.sh" --compile-only
# The optional avp_pc now links both wall and flat graphics passes and
# requires the flat shader as a build dependency. --run probes the real ROM.
if [[ "$mode" == "--gpu" ]]; then
    shift
    echo 'Starting original source-backed Vulkan FLOOR + CEILING + WALL GPU readback.'
    exec bash "$source_dir/scripts/verify_vulkan_gameplay_probe_r353.sh" --run "$@"
fi
