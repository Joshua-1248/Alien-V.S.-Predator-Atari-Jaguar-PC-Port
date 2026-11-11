#!/usr/bin/env bash
# RE #35 R87: Release both Linux compilers WITH original-Jaguar title emulation.
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${1:-${SOURCE_DIR}/release_linux_original_title}"
mkdir -p "${OUTPUT_DIR}" "${OUTPUT_DIR}/LINUX"

build_one() {
  local compiler="$1" compiler_cxx="$2" bin="$3" directory="$4"
  cmake -S "${SOURCE_DIR}" -B "${directory}" \
    -DCMAKE_C_COMPILER="${compiler}" -DCMAKE_CXX_COMPILER="${compiler_cxx}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVP_BUILD_PC=ON -DAVP_BUILD_LOCAL_MULTIPLAYER=ON \
    -DAVP_BUILD_JAG_REFERENCE=ON -DAVP_STRICT=ON
  cmake --build "${directory}" --target avp_pc --parallel "${AVP_BUILD_JOBS:-4}"
  # The first-boot Jaguar title requires BOTH the native bridge and Musashi.
  grep -q '^AVP_BUILD_JAG_REFERENCE:BOOL=ON$' "${directory}/CMakeCache.txt"
  grep -q 'AVP_PC_NATIVE_TITLE_JAGM=1' "${directory}/CMakeFiles/avp_pc.dir/flags.make"
  nm "${directory}/avp_pc" | awk '$2 == "T" && $3 == "m68k_execute" {ok=1} END {exit !ok}'
  nm "${directory}/avp_pc" | awk '$2 == "T" && $3 == "avp_pc_title_native_present" {ok=1} END {exit !ok}'
  cp "${directory}/avp_pc" "${OUTPUT_DIR}/LINUX/${bin}"
  chmod +x "${OUTPUT_DIR}/LINUX/${bin}"
}

build_one gcc g++ avp_pc "${OUTPUT_DIR}/build_gcc"
build_one clang clang++ avp_pc_clang "${OUTPUT_DIR}/build_clang"
sha256sum "${OUTPUT_DIR}/LINUX/avp_pc" "${OUTPUT_DIR}/LINUX/avp_pc_clang" \
  > "${OUTPUT_DIR}/SHA256SUMS.txt"
echo "Both Linux builds contain the R82/R83 Jaguar native title bridge."
