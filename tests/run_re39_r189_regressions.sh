#!/usr/bin/env bash
# RE39 R189: corrected legacy fixture and protected AMP/Predator smoke tests.
# No retail ROM, audio device, X11 server, or network connection required.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-cc}"
WORK="$(mktemp -d /tmp/avp-r189-tests.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
FLAGS=(-std=c99 -O1 -Wall -Wextra -Werror -I"$ROOT/include")
count=0
run(){
    local label="$1"; shift
    "$CC" "${FLAGS[@]}" "$ROOT/tests/$label.c" "$@" -o "$WORK/$label" -lm
    "$WORK/$label"
    printf 'PASS %s\n' "$label"
    count=$((count+1))
}
run test_levels_doors "$ROOT/src/game/levels.c" "$ROOT/src/game/doors.c"
run test_mjp "$ROOT/src/game/mjp.c" "$ROOT/src/game/runtime.c"
run test_frontend_renderer "$ROOT/new/jaguar/avp_gpu/frontend_renderer.c" \
    "$ROOT/new/jaguar/avp_gpu/frontend_fade.c" "$ROOT/new/jaguar/hw/cry.c" \
    "$ROOT/new/jaguar/hw/framebuffer.c"
run test_host_audio "$ROOT/new/jaguar/host/fullsynth_host.c" \
    "$ROOT/new/jaguar/host/jerry_wave_procedural.c" -pthread
run test_fx_tfx_volume_store_r272 "$ROOT/new/jaguar/host/jerry_wave_procedural.c" -pthread
run test_pc_audio_alsa_recovery -D_POSIX_C_SOURCE=200809L \
    "$ROOT/new/pc_host/src/pc_audio_layout.c" \
    "$ROOT/new/jaguar/host/fullsynth_host.c" \
    "$ROOT/new/jaguar/host/jerry_wave_procedural.c" -pthread -ldl
run test_pc_audio_pulse_failure -D_POSIX_C_SOURCE=200809L \
    "$ROOT/new/pc_host/src/pc_audio_layout.c" \
    "$ROOT/new/jaguar/host/fullsynth_host.c" \
    "$ROOT/new/jaguar/host/jerry_wave_procedural.c" -pthread -ldl
run test_sprite_row_sample_cache -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections \
    "$ROOT/new/jaguar/avp_gpu/maze2_occlusion.c" "$ROOT/new/jaguar/hw/cry.c" \
    "$ROOT/new/jaguar/avp_gpu/thermal_vision.c" -Wl,--gc-sections
run test_sprite_column_cache -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections \
    "$ROOT/new/jaguar/avp_gpu/maze2_occlusion.c" "$ROOT/new/jaguar/hw/cry.c" \
    "$ROOT/new/jaguar/avp_gpu/thermal_vision.c" -Wl,--gc-sections
# F12/input unit tests compile the real production X11 module; only
# unrelated host/game backends use link-only fixtures.  No X server needed.
X11_FLAGS=(-O1 -D_DEFAULT_SOURCE -DAVP_LOCAL_MULTIPLAYER
    -I"$ROOT/new/game_micro_optimization" -I"$ROOT/new/map_maker"
    -I"$ROOT/new/multiplayer" -ffunction-sections -fdata-sections)
run test_pc_short_press_events_r264 "${X11_FLAGS[@]}" -Wl,--gc-sections -lX11
run test_pc_terminal_mouse_weapon_cycle_r190 "${X11_FLAGS[@]}" \
    "$ROOT/tests/pc_menu_link_stubs_r189.c" -Wl,--gc-sections -lX11
run test_pc_focus_buffer_reset_r265 "${X11_FLAGS[@]}" \
    "$ROOT/tests/pc_menu_link_stubs_r189.c" -Wl,--gc-sections -lX11
run test_pc_quick_turn_focus_r270 "${X11_FLAGS[@]}" \
    "$ROOT/tests/pc_menu_link_stubs_r189.c" -Wl,--gc-sections -lX11
run test_pc_focus_short_buffer_r271 "${X11_FLAGS[@]}" \
    "$ROOT/tests/pc_menu_link_stubs_r189.c" -Wl,--gc-sections -lX11
run test_pc_controls_r257 "${X11_FLAGS[@]}" \
    "$ROOT/tests/pc_menu_link_stubs_r189.c" -Wl,--gc-sections -lX11
run test_pc_analog_settings_r259 "${X11_FLAGS[@]}" \
    "$ROOT/tests/pc_menu_link_stubs_r189.c" -Wl,--gc-sections -lX11
run test_amp_facehug_start_lifecycle_r184 -ffunction-sections -fdata-sections \
    "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" \
    -Wl,--gc-sections
run test_amp_queen_shield_shutdown_r186 -ffunction-sections -fdata-sections \
    "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" \
    -Wl,--gc-sections
run test_amp_state_sweep_longword_r188 -ffunction-sections -fdata-sections \
    "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -Wl,--gc-sections
run test_predator_background_r166 "$ROOT/new/jaguar/avp_gpu/thermal_vision.c"
run test_predator_infrared_r164 "$ROOT/new/jaguar/avp_gpu/thermal_vision.c"
# RE39 R191: exact 68000 word ALU across the signed word domain plus
# production acid/shield/spark/facehug update paths.
run test_amp_word_alu_r191 -ffunction-sections -fdata-sections \
    "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -Wl,--gc-sections
# RE39 R192: register sign-extension, cloaked-search counter, and AMP saves.
run test_amp_register_words_r192 -ffunction-sections -fdata-sections \
    "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -Wl,--gc-sections
run test_amp_score_wrap_r154 -ffunction-sections -fdata-sections \
    "$ROOT/src/game/amp.c" -Wl,--gc-sections
printf 'R192 focused regression sweep: %d/%d passed (%s)\n' "$count" "$count" "$CC"
