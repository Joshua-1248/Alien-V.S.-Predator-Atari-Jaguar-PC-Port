#!/usr/bin/env bash
# RE47 R347: opt-in, non-game Vulkan shader/GPU validation on Linux/X11.
# This compiles the SDK-gated diagnostics and runs real image readback tests.
# It NEVER enables Hardware gameplay and never installs system packages.
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${AVP_VULKAN_DIAG_BUILD_DIR:-${source_dir}/build-vulkan-diagnostic-r347}"
mode="${1:---gpu}"
if [[ "$mode" != "--gpu" && "$mode" != "--compile-only" ]]; then
    echo 'Usage: scripts/verify_vulkan_wall_gpu_r347.sh [--gpu|--compile-only]' >&2
    echo 'Set AVP_VULKAN_DIAG_BUILD_DIR to select a separate build folder.' >&2
    exit 2
fi
if ! command -v cmake >/dev/null || ! command -v glslangValidator >/dev/null; then
    echo 'Missing CMake or glslangValidator; install the system Vulkan SDK and glslang-tools.' >&2
    exit 2
fi
if [[ "$mode" == '--gpu' && -z "${DISPLAY:-}" ]]; then
    echo 'No X11 DISPLAY; use --compile-only or run inside an X11 session.' >&2
    exit 2
fi
cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVP_BUILD_JAG_REFERENCE=ON \
    -DAVP_BUILD_VULKAN_WSI_DIAGNOSTIC=ON \
    -DAVP_BUILD_VULKAN_SHADER_DIAGNOSTIC=ON
cmake --build "$build_dir" --parallel 4 --target \
    avp_vulkan_wsi_smoke \
    avp_vk_projected_wall_shader_r336 \
    avp_vk_two_slot_wall_shader_r337
spv_dir="$build_dir/avp_vk_shader_r327"
if command -v spirv-val >/dev/null; then
    for spv in "$spv_dir"/*.spv; do
        spirv-val --target-env vulkan1.0 "$spv"
    done
fi
if [[ "$mode" == '--compile-only' ]]; then
    echo 'Vulkan WSI modules and shaders compiled (not GPU-executed).'
    exit 0
fi
# Distinguish inaccessible GPUs (diagnostic exit 77) from a successful test.
# Do not let CTest's SKIP_RETURN_CODE turn a missing device into a PASS.
run_gpu_case() {
    local title="$1" flag="$2" fragment="$3" code
    echo "Vulkan GPU validation: $title"
    set +e
    "$build_dir/avp_vulkan_wsi_smoke" "$flag" \
        "$spv_dir/avp_diag_opaque.vert.spv" "$spv_dir/$fragment"
    code=$?
    set -e
    if [[ "$code" -eq 77 ]]; then
        echo "GPU check unavailable ($title): no usable X11/Vulkan GPU." >&2
        return 77
    fi
    if [[ "$code" -ne 0 ]]; then
        echo "GPU check FAILED ($title): exit code $code." >&2
        return "$code"
    fi
}
run_gpu_case 'projected plain walls' --projected-wall \
    avp_diag_projected_wall.frag.spv
run_gpu_case 'two texture slots' --two-slot-wall \
    avp_diag_two_slot_wall.frag.spv
echo 'Projected-wall GPU readback passed; two-slot GPU output matched BOTH the independent source-backed R347 oracle and legacy reference.'
echo 'NOTE: this is NOT complete gameplay rendering or full Jaguar parity.'
