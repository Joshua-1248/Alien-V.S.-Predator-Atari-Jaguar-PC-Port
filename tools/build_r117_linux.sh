#!/usr/bin/env bash
# Reproducible Linux R117 build from this complete SOURCE directory.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
for compiler in gcc clang; do
  if [ "$compiler" = gcc ]; then
    cc=gcc; cxx=g++; output=avp_pc
  else
    cc=clang; cxx=clang++; output=avp_pc_clang
  fi
  b="$root/build-$compiler"
  CC="$cc" CXX="$cxx" cmake -S "$root" -B "$b" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON \
    -DAVP_BUILD_LOCAL_MULTIPLAYER=ON -DAVP_BUILD_JAG_REFERENCE=ON
  cmake --build "$b" -j 4 --target \
    avp_pc test_local_mp_bot test_local_mp_cheat_targets_r114 \
    test_local_mp_session_options_r114 test_jerry_alarm_volume_r114
  "$b/new/multiplayer/test_local_mp_bot"
  "$b/test_local_mp_cheat_targets_r114"
  "$b/test_local_mp_session_options_r114"
  "$b/test_jerry_alarm_volume_r114"
  nm "$b/avp_pc" | grep -q ' m68k_execute$'
  nm "$b/avp_pc" | grep -q ' avp_pc_title_native_create$'
  nm "$b/avp_pc" | grep -q ' avp_pc_title_native_present$'
  mkdir -p "$root/dist/LINUX"
  install -m 755 "$b/avp_pc" "$root/dist/LINUX/$output"
done
printf '\nBoth Linux executables rebuilt in %s/dist/LINUX\n' "$root"
