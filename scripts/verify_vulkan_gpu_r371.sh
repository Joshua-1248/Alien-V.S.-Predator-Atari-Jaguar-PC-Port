#!/usr/bin/env bash
# RE #48 / R371: opt-in real Vulkan WSI + shader GPU diagnostics.
# This script must NOT call a failed GPU test successful. It never needs AVP.jag.
set -u -o pipefail
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
bin="$here/avp_vulkan_wsi_smoke"
spv="$here/shaders"
report="${AVP_R371_GPU_REPORT:-$PWD/R371_VULKAN_GPU_DIAGNOSTIC.log}"
if [[ ! -x "$bin" ]]; then
  printf 'Missing Vulkan diagnostic binary: %s\n' "$bin" >&2
  exit 2
fi
for stage in avp_diag_opaque.vert.spv avp_diag_opaque.frag.spv \
             avp_diag_ray_columns.frag.spv avp_diag_two_slot_wall.frag.spv; do
  if [[ ! -s "$spv/$stage" ]]; then
    printf 'Missing SPIR-V shader: %s\n' "$spv/$stage" >&2
    exit 2
  fi
done
if [[ -z "${DISPLAY:-}" ]]; then
  echo 'No X11/XWayland DISPLAY; Vulkan Xlib diagnostics cannot run.' >&2
  exit 77
fi
: > "$report" || exit 2
printf 'AVP R371 real Vulkan GPU diagnostics (requires Vulkan ICD/driver and X11)\n' | tee -a "$report"
printf 'Binary: %s\nShader directory: %s\n' "$bin" "$spv" | tee -a "$report"
extra=()
if [[ -n "${AVP_VULKAN_GPU_KEY:-}" ]]; then
  extra+=("$AVP_VULKAN_GPU_KEY")
fi
run_case() {
  local label="$1" rc
  shift
  printf '\n== %s ==\n' "$label" | tee -a "$report"
  "$bin" "$@" "${extra[@]}" 2>&1 | tee -a "$report"
  rc=${PIPESTATUS[0]}
  if (( rc != 0 )); then
    printf 'FAILED/UNAVAILABLE: %s (exit %d). See %s\n' "$label" "$rc" "$report" | tee -a "$report" >&2
    return "$rc"
  fi
}
run_case 'WSI clear/present' || exit $?
run_case '12 persistent Vulkan frames' --loop 12 || exit $?
run_case 'GPU shader/readback parity' --shader-test \
  "$spv/avp_diag_opaque.vert.spv" "$spv/avp_diag_opaque.frag.spv" || exit $?
run_case 'GPU source ray-column parity' --ray-columns \
  "$spv/avp_diag_opaque.vert.spv" "$spv/avp_diag_ray_columns.frag.spv" || exit $?
run_case 'GPU dual-source wall parity' --two-slot-wall \
  "$spv/avp_diag_opaque.vert.spv" "$spv/avp_diag_two_slot_wall.frag.spv" || exit $?
printf '\nALL DIAGNOSTICS PASSED. Note: gameplay pixel parity is a separate test.\n' | tee -a "$report"
