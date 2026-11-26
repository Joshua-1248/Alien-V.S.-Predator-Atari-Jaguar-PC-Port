#!/usr/bin/env bash
# Atari Jaguar Alien vs Predator - R118. Retail ROM is NOT distributed.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
for compiler in gcc clang; do
  if [ "$compiler" = gcc ]; then cc=gcc; cxx=g++; output=avp_pc
  else cc=clang; cxx=clang++; output=avp_pc_clang; fi
  b="$root/build-$compiler"
  CC="$cc" CXX="$cxx" cmake -S "$root" -B "$b" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON \
    -DAVP_BUILD_LOCAL_MULTIPLAYER=ON -DAVP_BUILD_JAG_REFERENCE=ON
  cmake --build "$b" -j4 --target avp_pc test_local_mp_spycam \
    test_local_multiplayer_setup test_local_multiplayer_input \
    test_local_multiplayer_worlds test_local_mp_commands test_local_mp_bot \
    test_local_mp_cheat_targets_r114 test_local_mp_session_options_r114 \
    test_jerry_alarm_volume_r114
  for test in test_local_mp_spycam test_local_multiplayer_setup \
    test_local_multiplayer_input test_local_multiplayer_worlds \
    test_local_mp_commands test_local_mp_bot; do
    "$b/new/multiplayer/$test"
  done
  "$b/test_local_mp_cheat_targets_r114"
  "$b/test_local_mp_session_options_r114"
  "$b/test_jerry_alarm_volume_r114"
  nm "$b/avp_pc" > "$b/native-symbols.txt"
  grep -q ' m68k_execute$' "$b/native-symbols.txt"
  grep -q ' avp_pc_title_native_create$' "$b/native-symbols.txt"
  grep -q ' avp_pc_title_native_present$' "$b/native-symbols.txt"
  # The X11 smoke is optional because CI/console-only systems may not have
  # Xvfb or Xtst installed. Set AVP_TEST_SPYCAM_X11=1 to run it.
  if [ "${AVP_TEST_SPYCAM_X11:-0}" = 1 ] && command -v Xvfb >/dev/null 2>&1; then
    python3 "$root/new/multiplayer/tests/test_spycam_live_x11.py" \
      --exe "$b/avp_pc" --display "$([ "$compiler" = gcc ] && echo :193 || echo :194)"
  fi
  mkdir -p "$root/dist/LINUX"
  install -m 755 "$b/avp_pc" "$root/dist/LINUX/$output"
done
printf 'Complete: %s/dist/LINUX/avp_pc + avp_pc_clang\n' "$root"
