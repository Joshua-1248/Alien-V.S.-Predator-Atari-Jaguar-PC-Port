#!/usr/bin/env bash
# RE35 R88: release both Linux compilers together and NEVER drop native title.
set -euo pipefail
source_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${1:-$source_dir/release_linux_re35_r88}"
mkdir -p "$out_dir/LINUX"

build() {
  local cc="$1" cxx="$2" filename="$3" path="$4"
  cmake -G Ninja -S "$source_dir" -B "$path" \
    -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVP_BUILD_PC=ON -DAVP_BUILD_LOCAL_MULTIPLAYER=ON \
    -DAVP_BUILD_JAG_REFERENCE=ON -DAVP_STRICT=ON
  cmake --build "$path" --target avp_pc test_intro_rgb16_cross_continuity_r88 \
    test_title_menu_aperture_r83 test_title_f2_attract_guard \
    --parallel "${AVP_BUILD_JOBS:-4}"
  grep -q '^AVP_BUILD_JAG_REFERENCE:BOOL=ON$' "$path/CMakeCache.txt"
  grep -q -- '-DAVP_PC_NATIVE_TITLE_JAGM=1' "$path/build.ninja"
  nm "$path/avp_pc" | awk '$2 == "T" && $3 == "m68k_execute" {found=1} END {exit !found}'
  nm "$path/avp_pc" | awk '$2 == "T" && $3 == "avp_pc_title_native_present" {found=1} END {exit !found}'
  "$path/test_intro_rgb16_cross_continuity_r88"
  "$path/test_title_menu_aperture_r83"
  "$path/test_title_f2_attract_guard"
  cp "$path/avp_pc" "$out_dir/LINUX/$filename"
  chmod +x "$out_dir/LINUX/$filename"
}

build gcc g++ avp_pc "$out_dir/build_gcc"
build clang clang++ avp_pc_clang "$out_dir/build_clang"
for name in LICENSE.md CREDITS.md PROVENANCE.md THIRD_PARTY_NOTICES.md README_RE35_R88_ATTRACT_RGB16_AND_ROM_TRACE.md; do
  cp "$source_dir/$name" "$out_dir/$name"
done
(cd "$out_dir" && sha256sum LINUX/avp_pc LINUX/avp_pc_clang > SHA256SUMS.txt)
echo "PASS: GCC and Clang bundled with authenticated native Jaguar title path"
