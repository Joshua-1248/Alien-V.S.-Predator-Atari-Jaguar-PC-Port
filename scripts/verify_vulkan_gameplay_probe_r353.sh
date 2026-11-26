#!/usr/bin/env bash
# RE47/R353: build the OPTIONAL gameplay-to-Vulkan live-wall probe. Running it
# requires the user's own Jaguar ROM and Vulkan/X11 driver. Normal game stays
# on the Software renderer; GPU output is an isolated one-frame diagnostic.
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${AVP_VULKAN_GAME_PROBE_BUILD_DIR:-${source_dir}/build-vulkan-game-probe-r353}"
mode="${1:---compile-only}"
if [[ "$mode" != "--compile-only" && "$mode" != "--run" ]]; then
    echo 'Usage: scripts/verify_vulkan_gameplay_probe_r353.sh [--compile-only|--run [AVP_GAME_ARGS...]]' >&2
    exit 2
fi
if ! command -v glslangValidator >/dev/null; then
    echo 'Please source the official Vulkan SDK setup-env.sh first.' >&2
    exit 2
fi
cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DAVP_BUILD_JAG_REFERENCE=ON \
    -DAVP_BUILD_VULKAN_WSI_DIAGNOSTIC=ON \
    -DAVP_BUILD_VULKAN_SHADER_DIAGNOSTIC=ON \
    -DAVP_BUILD_VULKAN_GAME_PROBE=ON
cmake --build "$build_dir" --parallel 4 --target avp_pc
if [[ "$mode" == "--compile-only" ]]; then
    echo 'Optional real-game Vulkan probe compiled. Normal game executable unaffected.'
    exit 0
fi
shift
export AVP_VULKAN_GAMEPLAY_WALL_PROBE=1
export AVP_VULKAN_WALL_SPV_DIR="$build_dir/avp_vk_shader_r327"
echo 'Launching AVP in SOFTWARE mode with a one-frame opt-in Vulkan wall probe.'
exec "$build_dir/avp_pc" "$@"
