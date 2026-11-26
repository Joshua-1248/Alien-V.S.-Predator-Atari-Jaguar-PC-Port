#!/usr/bin/env bash
# R368: opt-in Vulkan game executable plus its required runtime SPIR-V stages.
# Never replaces the software avp_pc or avp_pc_clang release binaries.
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${AVP_VULKAN_BUILD_DIR:-$repo/build-vulkan-experimental}"
dist="${AVP_VULKAN_OUTPUT_DIR:-$repo/dist/vulkan-experimental}"
name="${AVP_VULKAN_OUTPUT_BINARY:-avp_pc_vulkan}"
case "$name" in avp_pc_vulkan|avp_pc_vulkan_clang) ;; *) echo 'Unexpected Vulkan output name' >&2; exit 2;; esac
# The caller may select CC=clang CXX=clang++ or CC=gcc CXX=g++.
cmake -S "$repo" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON \
  -DAVP_BUILD_JAG_REFERENCE=ON -DAVP_BUILD_LOCAL_MULTIPLAYER=ON \
  -DAVP_BUILD_VULKAN_WSI_DIAGNOSTIC=ON \
  -DAVP_BUILD_VULKAN_SHADER_DIAGNOSTIC=ON \
  -DAVP_BUILD_VULKAN_GAME_PROBE=ON
cmake --build "$build" --parallel "${AVP_BUILD_JOBS:-4}" --target \
  avp_pc avp_vk_hybrid_present_shader_r361 test_pc_hybrid_shader_paths_r368
"$build/test_pc_hybrid_shader_paths_r368"
"$build/avp_pc" --help >/dev/null
mkdir -p "$dist/shaders"
install -m 755 "$build/avp_pc" "$dist/$name"
for shader in avp_diag_opaque.vert.spv avp_hybrid_world.frag.spv avp_hybrid_present_overlay.frag.spv; do
  test -s "$build/avp_vk_shader_r327/$shader"
  install -m 644 "$build/avp_vk_shader_r327/$shader" "$dist/shaders/$shader"
done
(
  cd "$dist/shaders"
  sha256sum avp_diag_opaque.vert.spv avp_hybrid_world.frag.spv avp_hybrid_present_overlay.frag.spv > SHA256SUMS
)
cat > "$dist/README_VULKAN_EXPERIMENTAL.txt" <<'TXT'
Alien vs Predator (Atari Jaguar) -- EXPERIMENTAL Vulkan-enabled Linux build

Run from the directory containing this executable (or use an absolute path):
  AVP_VULKAN_HYBRID_PRESENT=1 ./avp_pc_vulkan
  # Or, for the alternate compiler: ./avp_pc_vulkan_clang

In F12, select Hardware/Hybrid Vulkan; otherwise the game uses Software.
The shaders/ directory MUST accompany the executable. Never install these
SPIR-V stages into the system Vulkan SDK or copy them over the ROM.

The normal software Linux executables remain separate and unchanged. Hardware
rendering is currently an opt-in developer implementation with a full Jaguar
Software fallback, NOT a production feature. Supported scenes still render
the Software reference for strict comparison; expect limited or no speedup.
Unsupported scenes, overlays and multi-player stay Software. Actual driver,
GPU-output parity and X11 presentation remain unverified by the CI compile.

Use only your own AVP.jag; it and proprietary texture assets are not included.
Requires a compatible host Vulkan driver/loader at runtime. SDK headers and
glslang are needed only to *build* this executable and its shader pack.
TXT
echo "Packaged experimental Vulkan game and external SPIR-V to: $dist"
