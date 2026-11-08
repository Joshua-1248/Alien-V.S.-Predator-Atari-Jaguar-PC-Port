#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-cc}"
CFLAGS=(-std=c99 -O2 -Wall -Wextra -Werror -I"$ROOT/include")
build_run(){ local name=$1; shift; "$CC" "${CFLAGS[@]}" "$ROOT/tests/$name.c" "$@" -o "/tmp/$name" -lm; "/tmp/$name"; }
# S40: unsupported panel special/notice paths must not become fabricated loads.
build_run test_panels_cached_s40 "$ROOT/src/jaguar/avp_gpu/panels_cached.c" "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
# S41: original non-mapped notice semantics and explicit unsupported gates.
build_run test_panels_notice_s41 "$ROOT/src/jaguar/avp_gpu/panels_cached.c" "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
# R255: signed 32-bit Alien bite GPU snapshot offsets retain 68000 modulo-32 behavior.
build_run test_alien_bite_snapshot_r255 "$ROOT/src/game/maze.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
python3 "$ROOT/tests/test_pc_alien_bite_snapshot_r255.py"
build_run test_hud_message "$ROOT/src/game/hud_message.c"
# R286: original PLAYER.S give_wep/fall-through vs independent pickup notice.
build_run test_hud_pickup_order_r286 "$ROOT/src/game/hud_message.c" "$ROOT/src/game/collectables.c" "$ROOT/src/game/runtime.c"
# R253: retail JOYED 32-bit ADD/SUB register wrap at editor limits.
build_run test_joyed_wrap_r253 "$ROOT/src/game/joyed.c"
build_run test_hud_original "$ROOT/src/game/hud.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R272: InitCocoons writes only cocoon_frame (stale EMPTY slot fields reach saves).
build_run test_init_cocoons_r272 "$ROOT/src/game/hud.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_automap "$ROOT/src/jaguar/avp_gpu/automap.c" "$ROOT/src/jaguar/avp_gpu/state.c" -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
# R276: original HUD-local automap is placed by the OP at screen Y+14.
build_run test_r276_automap_origin -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections
# R276: original notice panel JPEGs composite without substituting font glyphs.
build_run test_r276_notice_composite "$ROOT/src/jaguar/avp_gpu/notice_composite.c"
# R276: X and Y sprite projection follow independent viewport-to-retail dimensions.
build_run test_r276_sprite_aspect
build_run test_levels_doors "$ROOT/src/game/levels.c" "$ROOT/src/game/doors.c" "$ROOT/src/game/runtime.c"
# R249: door-grid 16.16 negative word coordinates and exhaustive s16 domain.
build_run test_door_grid_fixed_r249 -ffunction-sections -fdata-sections -Wl,--gc-sections
# R291: retail .xgen2 and ?genmode generator shutdown leaves grid owners intact.
build_run test_amp_generator_grid_r291 "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R250: 68000 wrapped door subtraction/NEG and generator count audio words.
build_run test_door_collision_delta_r250 -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_generator_alarm_word_r250 "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_campaign_end "$ROOT/src/game/maze.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_campaign_routes "$ROOT/src/game/door_retail_meta.c"
build_run test_campaign_terminals "$ROOT/src/game/computer.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pred_weapons "$ROOT/src/game/weapons.c"
# First-person Predator attack and medpak state use the real command interpreter.
build_run test_pred_wristblade "$ROOT/src/game/mazescrn.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pred_medkit_audio "$ROOT/src/game/mazescrn.c" "$ROOT/src/game/music.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_species_weapon_cycles "$ROOT/src/game/mazescrn.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pred_melee_hit "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R247: TestSpark original signed word / unsigned 32-bit CPU wrap (294 cases).
build_run test_player_shot_angle_wrap_r247 "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R249: 640 original TestSpark width-word cases, including signed negatives.
build_run test_player_shot_width_r249 "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pred_cloak "$ROOT/src/game/player.c" "$ROOT/src/game/music.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_player_ambient "$ROOT/src/game/player.c" "$ROOT/src/game/music.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_player_pain_option "$ROOT/src/game/player.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_collectables "$ROOT/src/game/collectables.c" "$ROOT/src/game/runtime.c"
build_run test_computer_tables
build_run test_pc_terminal_poll "$ROOT/src/jaguar/host/joypad_host.c"
build_run test_pc_terminal_roundtrip "$ROOT/src/game/computer.c" "$ROOT/src/game/runtime.c" "$ROOT/src/game/font.c" "$ROOT/src/game/objects.c" "$ROOT/src/jaguar/host/joypad_host.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R248: FONT.S line command packing must preserve signed word bits without C UB.
build_run test_font_line_packed_r248 "$ROOT/src/game/font.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pc_retail_terminal_timing "$ROOT/src/game/font.c" "$ROOT/src/game/computer.c" "$ROOT/src/game/runtime.c" "$ROOT/src/game/objects.c" "$ROOT/src/jaguar/host/joypad_host.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pc_retail_terminal_menu -D_DEFAULT_SOURCE "$ROOT/src/jaguar/avp_gpu/software_renderer.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_eeprom "$ROOT/src/game/eeprom.c"
# R278: retail Check_EE key $1234FAC3 (legacy PC key accepted on load) and 600000 default.
build_run test_eeprom_retail_key_r277 "$ROOT/src/game/eeprom.c"
# Use a fresh executable directory because PC EEPROM files deliberately live
# beside the executable. Keep tests away from players' actual save files.
AVP_EEPROM_TEST_DIR="$(mktemp -d /tmp/avp-eeprom-regression.XXXXXX)"
"$CC" "${CFLAGS[@]}" -D_DEFAULT_SOURCE "$ROOT/tests/test_pc_eeprom.c" "$ROOT/src/platform/pc/pc_eeprom.c" -o "$AVP_EEPROM_TEST_DIR/test_pc_eeprom"
"$AVP_EEPROM_TEST_DIR/test_pc_eeprom"
rm -r "$AVP_EEPROM_TEST_DIR"
# Verify the host CPU frame still follows MAZE.S NextFrame/PlayAvP order.
python3 "$ROOT/tests/test_pc_gameplay_host_order.py"
python3 "$ROOT/tests/test_pc_postfirst_order.py"
# Retail title presentation enters the MJP menu automatically after 141 VBLs.
build_run test_title_automatic_menu -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_mjp "$ROOT/src/game/mjp.c" "$ROOT/src/game/runtime.c"
# R277: FX master fade advances per VBlank (retail $2EE80 hook), also during Do_Selec.
build_run test_fx_fade_vblank_r277 "$ROOT/src/game/music.c" "$ROOT/src/game/objects.c" "$ROOT/src/game/runtime.c"
# R184 original InitGame/ResetMaze host-entry primitives.
build_run test_campaign_restart_primitives "$ROOT/src/game/sprites.c" "$ROOT/src/game/mazescrn.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_music "$ROOT/src/game/music.c" "$ROOT/src/game/runtime.c"
# R272: DoEffect steals against the stored (quartered) type-4 priority word.
build_run test_music_type4_steal_r272 "$ROOT/src/game/music.c" "$ROOT/src/game/runtime.c"
build_run test_jerry_asr
build_run test_host_audio "$ROOT/src/jaguar/host/fullsynth_host.c" -lm -pthread
# R272: TFX store path quarters type-4 volume words and honours explicit D1.
build_run test_fx_tfx_volume_store_r272 -lm -pthread
"$CC" "${CFLAGS[@]}" -D_POSIX_C_SOURCE=200809L "$ROOT/tests/test_pc_audio_silent.c" \
    "$ROOT/src/platform/pc/pc_audio.c" "$ROOT/src/jaguar/host/fullsynth_host.c" \
    -o /tmp/test_pc_audio_silent -lm -lpthread -ldl
/tmp/test_pc_audio_silent
"$CC" "${CFLAGS[@]}" -D_POSIX_C_SOURCE=200809L "$ROOT/tests/test_pc_audio_alsa_recovery.c" \
    "$ROOT/src/jaguar/host/fullsynth_host.c" \
    -o /tmp/test_pc_audio_alsa_recovery -lm -lpthread -ldl
/tmp/test_pc_audio_alsa_recovery
"$CC" "${CFLAGS[@]}" -D_POSIX_C_SOURCE=200809L "$ROOT/tests/test_pc_audio_pulse_failure.c" \
    "$ROOT/src/jaguar/host/fullsynth_host.c" \
    -o /tmp/test_pc_audio_pulse_failure -lm -lpthread -ldl
/tmp/test_pc_audio_pulse_failure
build_run test_pc_winmm_resample "$ROOT/src/platform/pc/pc_winmm_resample.c"
build_run test_savegame "$ROOT/src/game/savegame.c" "$ROOT/src/game/eeprom.c" "$ROOT/src/game/music.c" "$ROOT/src/game/runtime.c"
# R273: original pause / save SFX use reserved DSP voices 0 / 1.
build_run test_pause_audio_voices_r273 "$ROOT/src/game/music.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R259: restored Marine tracker inventory survives PostFirst HUD recreation.
build_run test_marine_tracker_save_r259 "$ROOT/src/game/savegame.c" "$ROOT/src/game/mazescrn.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_rom_util "$ROOT/src/unzip/util_reconstructed.c"
build_run test_files "$ROOT/src/game/files.c"
# S30: original 68000 FILES archive + codec dispatch, independent of live loader.
build_run test_jagfiles_dispatch_s30 "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
# S31: original 68000 $BE00 JPEG quantization, exact 320-byte output.
build_run test_jagfiles_jpeg_quant_s31 "$ROOT/src/jaguar/avp_gpu/files_jpeg_init.c"
build_run test_jagfiles_jpeg_install_s32 "$ROOT/src/jaguar/avp_gpu/files_jpeg_init.c"
# RE25-S33: original GZIP mailbox/vector boundary; cartridge inflater is not a host shim.
build_run test_jagfiles_gzip_vectors_s33 "$ROOT/src/jaguar/avp_gpu/files_gzip_vectors.c" "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
# RE25-S35: native mode-2 JPEG dependency ordering and cache transition.
build_run test_jagfiles_jpeg_mode2_setup_s35 "$ROOT/src/jaguar/avp_gpu/files_jpeg_mode2_setup.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_mode2.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_init.c" "$ROOT/src/jaguar/avp_gpu/files_gzip_vectors.c" "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
build_run test_jagfiles_jpeg_dispatch_s36 "$ROOT/src/jaguar/avp_gpu/files_jpeg_dispatch.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_mode2_setup.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_mode2.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_init.c" "$ROOT/src/jaguar/avp_gpu/files_gzip_vectors.c" "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
# RE25-S34: isolated nested JPEG mode-2 CRY table through real FILES/GZIP boundaries.
build_run test_jagfiles_jpeg_mode2_s34 "$ROOT/src/jaguar/avp_gpu/files_jpeg_mode2.c" "$ROOT/src/jaguar/avp_gpu/files_gzip_vectors.c" "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
build_run test_rom_bootstrap "$ROOT/src/unzip/rom_bootstrap.c"
# R290: integrated production LEVELS::KillBastards + real AMP allocator;
# original LEVELS.S mode-only cleanup (R278 ASM audit), no slot/grid release.
build_run test_amp_spawn_allocator_r289 "$ROOT/src/game/amp.c" "$ROOT/src/game/levels.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R293: CURAMPNO is the executing actor in the retail update loop, not
# necessarily the passed AMP record. Templates outside the loop use ID.
build_run test_amp_curampno_r293 "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_amp "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
build_run test_enemy_marine "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
# R268: final Marine pulse damage checks original LOS only with optional enhancement.
build_run test_marine_pulse_los_r268 "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
# R269: final fatal pulse interruption uses source human death state immediately.
build_run test_marine_fire_interrupt_r269 "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
build_run test_enemy_recoil_fidelity "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
# Full Marine cocoon/Alien revival path, using original AMP and HUD state.
build_run test_alien_cocoon "$ROOT/src/game/amp.c" "$ROOT/src/game/hud.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# Exercise the actual Alien weapon damage + player hitscan + Marine AMP modes,
# including EEPROM-format cocoon save/reload and resurrection.
build_run test_alien_real_hits "$ROOT/src/game/amp.c" "$ROOT/src/game/mazescrn.c" "$ROOT/src/game/player.c" "$ROOT/src/game/hud.c" "$ROOT/src/game/savegame.c" "$ROOT/src/game/eeprom.c" "$ROOT/src/game/runtime.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# Use the real wall/door collision routine with the ordinary enemy chase:
# the $ffff placement sentinel must not be mistaken for an actor collision.
build_run test_amp_passage_geometry -DAVP_TEST_REAL_COLLISION -ffunction-sections -fdata-sections -Wl,--gc-sections "$ROOT/src/game/amp.c" "$ROOT/src/game/collision.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
build_run test_amp_projectile "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
build_run test_amp_routine_closure "$ROOT/src/game/amp.c" "$ROOT/src/game/runtime.c" "$ROOT/tests/amp_link_stubs.c"
build_run test_collision "$ROOT/src/game/collision.c"
# R251: Angle/Vector 68000 SUB.L modulo-32 overflow must retain R250 outputs.
build_run test_angle_vector_wrap_r251 "$ROOT/src/game/collision.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R252: area-damage metric preserves 68000 SUB/NEG/ASR/ADD wraparound.
build_run test_area_distance_wrap_r252 -ffunction-sections -fdata-sections -Wl,--gc-sections
"$CC" "${CFLAGS[@]}" -D_POSIX_C_SOURCE=200809L -DAVP_PC_HOST=1 -Dclock_gettime=avp_test_clock_gettime \
    "$ROOT/tests/test_pc_jaguar_mmio.c" "$ROOT/src/jaguar/host/jaguar_host.c" -o /tmp/test_pc_jaguar_mmio
/tmp/test_pc_jaguar_mmio
build_run test_jaguar_blitter_host "$ROOT/src/jaguar/host/blitter_host.c"
build_run test_jaguar_compat "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/object_processor.c" "$ROOT/src/jaguar/hw/framebuffer.c"
# RE24 Sweep 06: standalone TOM object-list-to-CRY16 scanline pipeline.
build_run test_op_scanline_pipeline "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/object_processor.c"
# RE24 Full Sweep 07: physical TOM double line-buffer and post-display BGEN.
build_run test_op_linebuffer_pair "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/object_processor.c"
# RE24 Full Sweep 08: TOM final VMODE CRY/RGB output mux, isolated.
build_run test_op_vmode_output "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/object_processor.c"
# RE24 Full Sweep 09: isolated 24-bit OP source phrase / physical RGB line output.
build_run test_op_rgb24_source "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/object_processor.c"
# Cross-check native OP decode against the original MJP CPU Make_New packing.
build_run test_op_mjp_source_crosswalk "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/object_processor.c"
build_run test_pred_vision_output "$ROOT/src/jaguar/hw/cry.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pred_vmode_native_spans "$ROOT/src/jaguar/hw/cry.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_pred_vmode_sample_columns "$ROOT/src/jaguar/hw/cry.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections
# R283: threaded host-resolution Predator VMODE/CLUT passes equal one sequential pass.
build_run test_pred_vision_threads_r283 "$ROOT/src/jaguar/hw/cry.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lpthread
# Compare all terminal artwork/text layers with R129 per-output-pixel raster.
build_run test_computer_native_composite -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_frontend_fade "$ROOT/src/jaguar/avp_gpu/frontend_fade.c"
build_run test_hud_fade
# R206: exact retail icon rectangles, cross-hatch and six-step selection fade.
build_run test_hud_weapon_icons
build_run test_pred_scope_raster
build_run test_pc_fps_meter
build_run test_pc_presentation_scale
build_run test_pc_fade_clock
# R178: New Game restores the original ROM maze bytes only when starting a
# new campaign; the synthetic regression is distributable without a ROM.
build_run test_pc_maze_restart "$ROOT/src/platform/pc/pc_resources.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
# R178: high-resolution SaveOvers must capture the last displayed world,
# not the old native frontend buffer left by character selection.
build_run test_pc_pause_world_snapshot "$ROOT/src/jaguar/hw/object_processor.c" "$ROOT/src/jaguar/hw/cry.c"
build_run test_frontend_renderer "$ROOT/src/jaguar/avp_gpu/frontend_renderer.c" "$ROOT/src/jaguar/avp_gpu/frontend_fade.c" "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/framebuffer.c"
# R279: retail title ALIEN logo settles at GPU phase 35; Legal_Pic FILES 233 bitmap.
build_run test_title_logo_legal_r279 "$ROOT/src/jaguar/avp_gpu/frontend_fade.c" "$ROOT/src/jaguar/hw/cry.c" "$ROOT/src/jaguar/hw/framebuffer.c"
# R280: retail CROSS stage kernels for the $01DF2C scene transition.
build_run test_scene_cross_r280 "$ROOT/src/jaguar/avp_gpu/frontend_fade.c" "$ROOT/src/jaguar/hw/cry.c"
build_run test_srcshade "$ROOT/src/jaguar/hw/cry.c"
build_run test_maze2_column "$ROOT/src/jaguar/avp_gpu/maze2_column.c"
build_run test_maze2_occlusion "$ROOT/src/jaguar/avp_gpu/maze2_occlusion.c"
build_run test_wall_raster_dda
build_run test_wall_texel_reuse "$ROOT/src/jaguar/avp_gpu/maze2_column.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
build_run test_tracker_brush "$ROOT/src/jaguar/avp_gpu/tracker_brush.c"
build_run test_tracker_layout -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections
build_run test_quickstate "$ROOT/src/platform/pc/pc_quickstate.c"
build_run test_fp_overlay "$ROOT/src/jaguar/avp_gpu/fp_overlay.c"
# R203: original Alien first move-in presentation remains the previous visible
# endpoint for smooth interpolation; Marine/Predator snapshots stay unchanged.
build_run test_alien_intro_snapshot "$ROOT/src/jaguar/avp_gpu/software_renderer.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
build_run test_sprite_sampling -lm
build_run test_sprite_span_clip -lm
# R135: exact R134 sprite draw comparison for cached vertical pixel-centre spans.
build_run test_sprite_row_sample_cache "$ROOT/src/jaguar/avp_gpu/maze2_occlusion.c" "$ROOT/src/jaguar/hw/cry.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
# Marine run front/rear facing (side/mirror path unchanged).
build_run test_marine_run_facing "$ROOT/src/jaguar/avp_gpu/maze2_occlusion.c" "$ROOT/src/jaguar/hw/cry.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
# Compare the actual R132 AAF sprite drawer with source-column row/color reuse.
build_run test_sprite_column_cache "$ROOT/src/jaguar/avp_gpu/maze2_occlusion.c" "$ROOT/src/jaguar/hw/cry.c" -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
build_run test_floor_cell_sampling -lm
# Compare the actual host floor raster against R126 per-pixel texture lookup.
build_run test_floor_texture_prebind -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
# The actual 320x228 HUD source sampler must match R133 at every pixel.
build_run test_hud_sample_columns -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
build_run test_overlay_scale_composite
build_run test_aaf_columns
build_run test_presentation_interp "$ROOT/src/jaguar/avp_gpu/presentation_interp.c"
# R257 PC control modes, deadzones and opt-in host-frame press buffer.
build_run test_pc_short_press_buffer
# R264: X11 press/release events inside one host poll still reach next tick.
build_run test_pc_short_press_events_r264 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R265: dropped window focus discards queued digital and analog impulses.
build_run test_pc_focus_buffer_reset_r265 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R270: new physical joystick edge after FocusOut must never queue Quick Turn.
build_run test_pc_quick_turn_focus_r270 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R271: FocusOut blocks newly arriving optional short input edges and pulses.
build_run test_pc_focus_short_buffer_r271 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
build_run test_pc_controls_r257 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R258: F12 Backspace backs out of categories, Delete clears bindings.
build_run test_pc_menu_back_delete_r258 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
build_run test_pc_input_routing
build_run test_pc_mouse_look -lm
# R259: independent one-shot axis buffer and unlimited mouse impulse.
build_run test_pc_analog_buffer_r259
build_run test_pc_analog_settings_r259 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# RE33 R59: real F12 On/Off switch, default disabled, persisted independently.
build_run test_normalize_diagonal_contact_menu_r59 -DAVP_LOCAL_MULTIPLAYER -D_DEFAULT_SOURCE \
    -I"$ROOT/new/multiplayer" -I"$ROOT/new/map_maker" \
    -I"$ROOT/new/game_micro_optimization" -I"$ROOT/src/platform/pc" \
    -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R268: F12 toggle + persisted on-disk Marine LOS option.
build_run test_marine_pulse_option_r268 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R269: separately persisted Marine Fire Interrupt F12 menu option.
build_run test_marine_fire_option_r269 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R266: PC-only interpolated clockwise half-turn, four binding slots and focus gating.
build_run test_pc_quick_turn_r266 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11 -lm
build_run test_pc_quick_turn_player_r266 "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
# R263: corrupt or non-finite saved turn configuration must not disable heading.
build_run test_pc_turn_settings_r263 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11 -lm
# R260: dedicated Enhancements category and 5.00 analog turn limits.
build_run test_f12_enhancements_limits_r260 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
build_run test_pc_mouse_unlimited_r259 -lm
build_run test_pc_player_aim_r259 "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
# R261: continuous Enhanced Analog heading retains sub-16-bit stop angles.
build_run test_pc_continuous_aim_r261 "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
build_run test_pc_continuous_renderer_r261 -lm
# R285: visual-only digital yaw and analog Smooth Presentation gate.
build_run test_pc_turn_presentation_r285
build_run test_pc_turn_keyboard_r285 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# S86 Player 1 joystick zoom appears in both original F12 button-binding pages.
build_run test_pc_p1_zoom_f12_s86 -I"$ROOT/new/multiplayer" -DAVP_LOCAL_MULTIPLAYER -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R262: the enhanced input angle keeps full precision in the game tick.
build_run test_pc_continuous_frame_r262 -lm
# R262 host-poll-rate analog heading, including direct production MovePlayer handoff.
build_run test_pc_live_heading_clock_r262 -lm
build_run test_pc_live_heading_player_r262 "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
python3 "$ROOT/tests/test_pc_enhancement_labels_r262.py"
# R261: independent PC Pause/Option stick buttons and edge-only mouse recenter.
build_run test_pc_fast_mouse_pause_option_r261 -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
# R260: mouse stops precisely near cardinal angles without disabling Jaguar snap.
build_run test_pc_mouse_idle_precision_r260 "$ROOT/src/game/player.c" -ffunction-sections -fdata-sections -Wl,--gc-sections -lm
build_run test_pc_analog_aim
build_run test_pc_joybinding
# R143: host-only Jaguar frontend menu key/button remapping and settings persistence.
build_run test_pc_frontend_bindings -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections -lX11
build_run test_pc_win32_keymap "$ROOT/src/platform/pc/pc_win32_keymap.c"
"$CC" "${CFLAGS[@]}" -fno-builtin -Dmalloc=avp_test_malloc "$ROOT/tests/test_inflate.c" "$ROOT/src/unzip/inflate_reconstructed.c" -o /tmp/test_inflate
/tmp/test_inflate
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/tests/test_projection.cpp" "$ROOT/src/jaguar/avp_gpu/projection.cpp" -o /tmp/test_projection
/tmp/test_projection
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
    "$ROOT/tests/test_panels.cpp" "$ROOT/src/jaguar/avp_gpu/panels.cpp" -o /tmp/test_panels
/tmp/test_panels
# R282: L3/L4 wall notice slots from LEVINFO.S / retail RDB.
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
    "$ROOT/tests/test_wall_notice_r282.cpp" "$ROOT/src/jaguar/avp_gpu/panels.cpp" -o /tmp/test_wall_notice_r282
/tmp/test_wall_notice_r282
# RE24-S10: TRM GPU/DSP RISC core, TRM Blitter, and the reference machine
# (synthetic cartridge only; the retail ROM is never needed by this suite).
build_run test_jrisc_trm "$ROOT/src/jaguar/hw/jrisc.c"
build_run test_blitter_trm "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/cry.c"
# RE24 Sweep 18: multi-subsystem SDK errata risk detection and OP scale bounds.
build_run test_atari_errata_s18 "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/object_processor.c" "$ROOT/src/jaguar/hw/cry.c"
# RE24 Sweep 17: Atari JDEV0002 A1/A2 Y-add erratum (all flags/signs, DSTA2, phrase/pixel).
build_run test_blitter_yadd_errata_s17 "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/cry.c"
"$CC" -std=c99 -O1 -w -I"$ROOT/third_party/musashi" -c "$ROOT/third_party/musashi/m68kcpu.c" -o /tmp/re24s10_m68kcpu.o
"$CC" -std=c99 -O1 -w -I"$ROOT/third_party/musashi" -c "$ROOT/third_party/musashi/m68kops.c" -o /tmp/re24s10_m68kops.o
"$CC" -std=c99 -O1 -w -I"$ROOT/third_party/musashi" -c "$ROOT/third_party/musashi/softfloat/softfloat.c" -o /tmp/re24s10_softfloat.o
build_run test_jag_machine_synthetic -I"$ROOT/third_party/musashi" "$ROOT/src/jaguar/refmachine/jag_machine.c" \
    "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/cry.c" \
    /tmp/re24s10_m68kcpu.o /tmp/re24s10_m68kops.o /tmp/re24s10_softfloat.o
# RE24 Sweep 18: synthetic 68000 writes two high-depth OP bitmap objects,
# one with RELEASE; test exposure counts without inventing DSP bus timing.
build_run test_op_bus_exposure_s18 -I"$ROOT/third_party/musashi" "$ROOT/src/jaguar/refmachine/jag_machine.c" \
    "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/cry.c" \
    /tmp/re24s10_m68kcpu.o /tmp/re24s10_m68kops.o /tmp/re24s10_softfloat.o
# RE24-S15: read-only GPU start/stop provenance, synthetic Motorola 68000
# register writes and read-only CSV summarizer; no cartridge required.
build_run test_jag_gpu_control_trace_s15 -I"$ROOT/third_party/musashi" "$ROOT/src/jaguar/refmachine/jag_machine.c" \
    "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/cry.c" \
    /tmp/re24s10_m68kcpu.o /tmp/re24s10_m68kops.o /tmp/re24s10_softfloat.o
PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/test_gpu_control_report_s15.py"
# RE24-S22: read-only GPU/Blitter physical DRAM page accounting and
# conservative no-resource-guesses report. Pure synthetic data only.
build_run test_gpu_view_access_s22 "$ROOT/src/jaguar/refmachine/gpu_view_access.c"
PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/test_gpu_view_access_report_s22.py"
PYTHONDONTWRITEBYTECODE=1 CC="$CC" python3 "$ROOT/tests/test_gpu_view_parity_compile_s22.py"
# GPU observer must see real GPU reads/writes from the reference machine,
# but not host/68000 accessors. Execute a fabricated 68000+JRISC cartridge.
build_run test_jag_gpu_view_bus_s22 -I"$ROOT/third_party/musashi" \
    "$ROOT/src/jaguar/refmachine/gpu_view_access.c" \
    "$ROOT/src/jaguar/refmachine/jag_machine.c" \
    "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/cry.c" \
    /tmp/re24s10_m68kcpu.o /tmp/re24s10_m68kops.o /tmp/re24s10_softfloat.o

# RE24-S23: Musashi-only DRAM writer provenance, filtered by real view reads.
build_run test_gpu_cpu_provenance_s23 -I"$ROOT/third_party/musashi" \
    "$ROOT/src/jaguar/refmachine/gpu_view_access.c" \
    "$ROOT/src/jaguar/refmachine/jag_machine.c" \
    "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/cry.c" \
    /tmp/re24s10_m68kcpu.o /tmp/re24s10_m68kops.o /tmp/re24s10_softfloat.o
PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/test_gpu_view_provenance_report_s23.py"
# RE24-S11: retail three-pass title-menu dim (verified on jagref).
build_run test_title_menu_dim_s11 "$ROOT/src/jaguar/avp_gpu/frontend_fade.c"
# RE24-S16: verify that parity checks cannot miss GPU flags/MAC/bank/division or PC drift.
build_run test_gpu_arch_parity_s16
# RE24-S13: JRISC->C static recompiler reproduces the TRM core exactly (synthetic program, no ROM data).
syn="/tmp/re24s13_syn"
python3 "$ROOT/tests/gen_gpu_recomp_synthetic_s13.py" "$syn"
build_run test_gpu_recomp_s13 -Wno-unused-label -Wno-implicit-fallthrough -DGEN_C="\"$syn.c\"" -DGEN_BIN="\"$syn.bin\"" "$ROOT/src/jaguar/hw/jrisc.c"
# RE24-S27: generated JRISC-to-C must follow the GPU FLAGS bank-select bit
# after STORE, otherwise original front-end JagPEG corrupts the IDCT bank.
PYTHONDONTWRITEBYTECODE=1 CC="$CC" python3 "$ROOT/tests/test_gpu_recomp_bank_refresh_s27.py"
# RE24-S19: native-view entrypoint, all-overlay validation, level-variable
# driven parameters and fail-closed execution (no cartridge required).
build_run test_gpu_original_native_s19 "$ROOT/src/jaguar/avp_gpu/original_view_native.c"
# RE24 S20: native 68000 gpu_* camera snapshot at authentic $7FB0;
# preserve the real game collision maze cells separately until their DRAM
# placement is verified.
build_run test_gpu_original_live_scene_s20 "$ROOT/src/jaguar/avp_gpu/original_view_scene.c" "$ROOT/src/game/collision.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
PYTHONDONTWRITEBYTECODE=1 CC="$CC" python3 "$ROOT/tests/test_gpu_original_live_scene_generated_s20.py"
# RE24 S21: caller-specified Jaguar DRAM layout, all-resource transaction and generated JRISC reads.
build_run test_gpu_original_scene_layout_s21 "$ROOT/src/jaguar/avp_gpu/original_view_layout.c" "$ROOT/src/jaguar/avp_gpu/original_view_native.c" -ffunction-sections -fdata-sections -Wl,--gc-sections
PYTHONDONTWRITEBYTECODE=1 CC="$CC" python3 "$ROOT/tests/test_gpu_original_scene_layout_generated_s21.py"
PYTHONDONTWRITEBYTECODE=1 CC="$CC" python3 "$ROOT/tests/test_gpu_original_native_generated_s19.py"
# RE24-S14: ROM-free, fail-closed overlay descriptor and per-level JRISC
# MOVEI parameterization tests. No generated cartridge instructions shipped.
PYTHONDONTWRITEBYTECODE=1 CC="$CC" python3 "$ROOT/tests/test_gpu_overlay_descriptors_s14.py"
# RE24 Session 2: native C port of the original 3D-view GPU program; TRM
# divider equivalence and a synthetic (ROM-free) scene through all stages.
# S28: deterministic, pointer-free Jaguar GPU view capture/replay, synthetic-only.
build_run test_view3d_snapshot_s28 "$ROOT/tools/jagref/view3d_snapshot.c"
build_run test_view3d_port_s25 "$ROOT/src/jaguar/avp_gpu/view3d.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/cry.c"
# RE24 S29: private capture/replay format for original JagPEG GPU decode/converter.
build_run test_jagpeg_snapshot_s29 "$ROOT/tools/jagref/view3d_snapshot.c"
# RE24 S26: native JagPEG 68000 preparation + GPU RGB-to-CRY ROM-free
# boundary checks. Retail decoder parity uses user-provided AVP.jag separately.
build_run test_jagpeg_s26 "$ROOT/src/jaguar/avp_gpu/jagpeg_gpu.c" "$ROOT/src/jaguar/avp_gpu/jagpeg_load.c" "$ROOT/src/jaguar/avp_gpu/view3d.c" "$ROOT/src/jaguar/hw/blitter_trm.c" "$ROOT/src/jaguar/hw/jrisc.c" "$ROOT/src/jaguar/hw/cry.c"
build_run test_re31_fixed_hud_hue "$ROOT/src/jaguar/hw/cry.c"
echo "All C-decomp regression tests passed."
# RE25-S37: original-native JagPEG expansion wired to the original FILES
# JPEG adapter. The GPU itself is stubbed HERE (real decoder has S25/S26 tests).
build_run test_jagfiles_jpeg_native_expand_s37 "$ROOT/src/jaguar/avp_gpu/files_jpeg_native_expand.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_dispatch.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_mode2_setup.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_mode2.c" "$ROOT/src/jaguar/avp_gpu/files_jpeg_init.c" "$ROOT/src/jaguar/avp_gpu/files_gzip_vectors.c" "$ROOT/src/jaguar/avp_gpu/files_dispatch.c"
