#!/usr/bin/env bash
# RE47/R355: validate the new source-backed floor/ceiling AVFL shader and
# capture real gameplay flat references. This DOES NOT draw flats on GPU yet;
# it only compiles the proposed shader and prepares the live transfer stream.
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${AVP_VULKAN_GAME_PROBE_BUILD_DIR:-${source_dir}/build-vulkan-game-probe-r353}"
mode="${1:---compile-only}"
if [[ "$mode" != "--compile-only" && "$mode" != "--run" ]]; then
    echo 'Usage: scripts/verify_vulkan_flat_gpu_r355.sh [--compile-only|--run [GAME_ARGS...]]' >&2
    exit 2
fi
if ! command -v glslangValidator >/dev/null; then
    echo 'Source the official LunarG Vulkan SDK setup-env.sh first.' >&2
    exit 2
fi
# The existing R353 game-probe script configures the same SDK/X11 project
# with the exact source-backed wall shader and the new flat shader target.
bash "$source_dir/scripts/verify_vulkan_gameplay_probe_r353.sh" --compile-only
cmake --build "$build_dir" --target avp_vk_flat_shader_r355 --parallel 2
shader="$build_dir/avp_vk_shader_r327/avp_diag_flat.frag.spv"
if [[ ! -s "$shader" ]]; then
    echo "Missing floor/ceiling SPIR-V output: $shader" >&2
    exit 1
fi
if command -v spirv-val >/dev/null; then
    spirv-val --target-env vulkan1.0 "$shader"
fi
echo 'R355 floor/ceiling shader compiled and validated (NO GPU drawing yet).'
if [[ "$mode" == "--run" ]]; then
    shift
    echo 'Starting SOFTWARE game + opt-in original live wall/flat source probe.'
    exec bash "$source_dir/scripts/verify_vulkan_gameplay_probe_r353.sh" --run "$@"
fi
