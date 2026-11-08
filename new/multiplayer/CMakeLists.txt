add_library(avp_local_multiplayer STATIC local_mp.c local_mp_input.c local_mp_mice.c local_mp_compositor.c local_mp_worlds.c local_mp_retail_maps.c local_mp_collision.c local_mp_move_guard.c local_mp_doors_native.c local_mp_player_native.c local_mp_weapon_native.c local_mp_actor_native.c local_mp_amp_native.c local_mp_amp_targets.c local_mp_native_context.c local_mp_native_input.c local_mp_commands.c local_mp_respawn_policy.c local_mp_life.c local_mp_deathmatch.c local_mp_dm_loot.c local_mp_corpses.c local_mp_transition.c local_mp_chamber.c local_mp_native_backend.c local_mp_gamepads.c local_mp_bot.c local_mp_bot_levels.c local_mp_walktest.c)
target_compile_definitions(avp_local_multiplayer PRIVATE AVP_LOCAL_MULTIPLAYER=1 AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE=1)
target_include_directories(avp_local_multiplayer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(avp_local_multiplayer PRIVATE -Wall -Wextra -Werror)
endif()
add_executable(test_local_multiplayer_setup tests/test_local_mp_setup.c)
target_link_libraries(test_local_multiplayer_setup PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_multiplayer_setup PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_local_multiplayer_input tests/test_local_mp_input.c)
target_link_libraries(test_local_multiplayer_input PRIVATE avp_local_multiplayer)
target_include_directories(test_local_multiplayer_input PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_multiplayer_input PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_local_mp_gamepads tests/test_local_mp_gamepads.c)
target_link_libraries(test_local_mp_gamepads PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_gamepads PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_local_mp_commands tests/test_local_mp_commands.c)
target_link_libraries(test_local_mp_commands PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_commands PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_local_multiplayer_compositor tests/test_local_mp_compositor.c)
target_link_libraries(test_local_multiplayer_compositor PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_multiplayer_compositor PRIVATE -Wall -Wextra -Werror)
endif()

# Host-only RE31 compositor equivalence to R2's exact division-based
# nearest-neighbor sampling, including all split layouts and terminal overlay.
add_executable(test_local_mp_compositor_exact tests/test_local_mp_compositor_exact.c)
target_link_libraries(test_local_mp_compositor_exact PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_compositor_exact PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_local_mp_chamber tests/test_local_mp_chamber.c)
target_link_libraries(test_local_mp_chamber PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_chamber PRIVATE -Wall -Wextra -Werror)
endif()
add_executable(test_local_multiplayer_worlds tests/test_local_mp_worlds.c)
target_link_libraries(test_local_multiplayer_worlds PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_multiplayer_worlds PRIVATE -Wall -Wextra -Werror)
endif()

# Host-level smoke: physically present one combined frame in ONE X11 window.
# Test renders distinct synthetic full frames solely to check the presentation
# contract; no simulated co-op gameplay, second windows or child processes.
# The production pc_video_x11.c presentation path is shared with Win32's
# existing Xlib compatibility shim, and has no OS-conditional MP algorithm.
if(UNIX AND NOT APPLE AND TARGET X11::X11)
  add_executable(test_local_mp_one_window_x11 tests/test_local_mp_one_window_x11.c)
  target_link_libraries(test_local_mp_one_window_x11 PRIVATE avp_local_multiplayer X11::X11)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_local_mp_one_window_x11 PRIVATE -Wall -Wextra -Werror)
  endif()
  # R15: exercise the real F12 video menu and the renderer's route choice.
  # The menu must be composed offscreen at all 21 presentation rates (not
  # just those above 60 Hz); no retail ROM or physical display is needed.
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(test_pc_f12_atomic_route
      ${CMAKE_SOURCE_DIR}/tests/test_pc_show_fps_menu.c
      ${CMAKE_SOURCE_DIR}/src/platform/pc/pc_original_cheat_input.c
      ${CMAKE_SOURCE_DIR}/src/jaguar/avp_gpu/automap.c
      ${CMAKE_SOURCE_DIR}/src/jaguar/avp_gpu/state.c
      ${CMAKE_SOURCE_DIR}/new/map_maker/map_maker.c
      ${CMAKE_SOURCE_DIR}/new/map_maker/map_maker_scripts.c
      ${CMAKE_SOURCE_DIR}/new/map_maker/map_maker_objects.c)
    target_include_directories(test_pc_f12_atomic_route PRIVATE
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/new/multiplayer
      ${CMAKE_SOURCE_DIR}/new/game_micro_optimization
      ${CMAKE_SOURCE_DIR}/new/map_maker)
    target_compile_definitions(test_pc_f12_atomic_route PRIVATE
      _POSIX_C_SOURCE=200809L AVP_LOCAL_MULTIPLAYER=1 AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE=1)
    target_compile_options(test_pc_f12_atomic_route PRIVATE
      -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
    target_link_options(test_pc_f12_atomic_route PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_pc_f12_atomic_route PRIVATE X11::X11 m ${CMAKE_DL_LIBS})
  endif()
  # R12: real X11 XShm parity for the deathmatch banner on direct/window
  # versus the R11 full back-pixmap presentation, with FPS text overlap.
  add_executable(test_pc_dm_status_direct_x11 tests/test_pc_dm_status_direct_x11.c)
  target_link_libraries(test_pc_dm_status_direct_x11 PRIVATE X11::X11)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_dm_status_direct_x11 PRIVATE -Wall -Wextra -Werror)
  endif()
endif()

# Independent real retail-maze copies and campaign persistence; no engine globals.
add_executable(test_local_mp_retail_maps tests/test_local_mp_retail_maps.c)
target_link_libraries(test_local_mp_retail_maps PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_retail_maps PRIVATE -Wall -Wextra -Werror)
endif()

# Optional original-ROM resource audit, never distributes the user-owned ROM.
# Retails's decoder lives in the existing game; function-section GC removes
# unrelated ROM-to-game bindings to avoid pulling in the global game engine.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_retail_rom
    tests/test_local_mp_retail_rom.c
    ${CMAKE_SOURCE_DIR}/src/jaguar/avp_resources/retail_resources.c
    ${CMAKE_SOURCE_DIR}/src/game/collision.c
    ${CMAKE_SOURCE_DIR}/src/game/doors.c
    ${CMAKE_SOURCE_DIR}/src/game/levels.c)
  target_include_directories(test_local_mp_retail_rom PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_retail_rom PRIVATE avp_local_multiplayer ZLIB::ZLIB)
  target_compile_options(test_local_mp_retail_rom PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_retail_rom PRIVATE -Wl,--gc-sections)
endif()

# Original COLLIDE.S runtime parity in a single process/window: exercise the
# unmodified reconstructed collision.c and InitDoors() against scoped retail
# maps. GC removes unrelated game modules from this small test executable.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_collision_original
    tests/test_local_mp_collision_original.c
    ${CMAKE_SOURCE_DIR}/src/game/collision.c
    ${CMAKE_SOURCE_DIR}/src/game/doors.c)
  target_include_directories(test_local_mp_collision_original PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_collision_original PRIVATE avp_local_multiplayer m)
  target_compile_options(test_local_mp_collision_original PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_collision_original PRIVATE -Wl,--gc-sections)
endif()

# The door-machine snapshot extension is an optional, isolated include at the
# tail of doors.c. Match that flag in the small original-engine integration test.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_doors_native
    tests/test_local_mp_doors_native.c
    ${CMAKE_SOURCE_DIR}/src/game/doors.c)
  target_include_directories(test_local_mp_doors_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_doors_native PRIVATE avp_local_multiplayer)
  target_compile_definitions(test_local_mp_doors_native PRIVATE AVP_LOCAL_MULTIPLAYER_DOOR_BRIDGE=1)
  target_compile_options(test_local_mp_doors_native PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_doors_native PRIVATE -Wl,--gc-sections)
endif()

# Original PLAYER.S runtime-state capture. Test links the real player.c and
# exercises real original movement and RNG routines; never a substitute model.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_player_native
    tests/test_local_mp_player_native.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c)
  target_include_directories(test_local_mp_player_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_player_native PRIVATE avp_local_multiplayer m)
  target_compile_definitions(test_local_mp_player_native PRIVATE AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE=1)
  target_compile_options(test_local_mp_player_native PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_player_native PRIVATE -Wl,--gc-sections)
endif()

# Original MAZESCRN.S first-person script machine and WEAPONS.S Predator
# private unlock state, with unused engine sections GC'd from the focused test.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_weapon_native
    tests/test_local_mp_weapon_native.c
    ${CMAKE_SOURCE_DIR}/src/game/mazescrn.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c)
  target_include_directories(test_local_mp_weapon_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_weapon_native PRIVATE avp_local_multiplayer m)
  target_compile_definitions(test_local_mp_weapon_native PRIVATE AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE=1)
  target_compile_options(test_local_mp_weapon_native PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_weapon_native PRIVATE -Wl,--gc-sections)
endif()

# Native combined actor transaction. All three original C units run in the
# same executable; outputs/physics *unused* by this focused test are link stubs.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_actor_native
    tests/test_local_mp_actor_native.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/mazescrn.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/hud_message.c
    ${CMAKE_SOURCE_DIR}/src/game/hud.c)
  target_include_directories(test_local_mp_actor_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_actor_native PRIVATE avp_local_multiplayer m)
  target_compile_definitions(test_local_mp_actor_native PRIVATE
    AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_HUDMSG_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_HUDVIEW_BRIDGE=1)
  target_compile_options(test_local_mp_actor_native PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_actor_native PRIVATE -Wl,--gc-sections)
endif()

# Original AMP.S allocator, grids, persistent placement, NPC records and private
# per-world counters. Focused test links the original amp.c only.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_amp_native
    tests/test_local_mp_amp_native.c
    ${CMAKE_SOURCE_DIR}/src/game/amp.c)
  target_include_directories(test_local_mp_amp_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_amp_native PRIVATE avp_local_multiplayer)
  target_compile_definitions(test_local_mp_amp_native PRIVATE AVP_LOCAL_MULTIPLAYER_AMP_BRIDGE=1)
  target_compile_options(test_local_mp_amp_native PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_amp_native PRIVATE -Wl,--gc-sections)
  # R13: exercise the same original AMP allocator capture/restore pipeline
  # with the optional read-only bitmap validator wired in, and then removed.
  add_executable(test_game_micro_amp_scope
    tests/test_local_mp_amp_native.c
    ${CMAKE_SOURCE_DIR}/src/game/amp.c
    ${CMAKE_SOURCE_DIR}/new/game_micro_optimization/amp_free_list.c)
  target_include_directories(test_game_micro_amp_scope PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/new/game_micro_optimization)
  target_link_libraries(test_game_micro_amp_scope PRIVATE avp_local_multiplayer)
  target_compile_definitions(test_game_micro_amp_scope PRIVATE
    AVP_LOCAL_MULTIPLAYER_AMP_BRIDGE=1 AVP_TEST_FAST_AMP_VALIDATOR=1)
  target_compile_options(test_game_micro_amp_scope PRIVATE
    -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_amp_scope PRIVATE -Wl,--gc-sections)
  # R14: benchmark actual original AMP capture with R13 bitmap validation
  # and new opt-in direct destination capture, on the same live state.
  add_executable(bench_game_micro_amp_capture
    ${CMAKE_SOURCE_DIR}/new/game_micro_optimization/tests/bench_amp_capture.c
    ${CMAKE_SOURCE_DIR}/src/game/amp.c
    ${CMAKE_SOURCE_DIR}/new/game_micro_optimization/amp_free_list.c)
  target_include_directories(bench_game_micro_amp_capture PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/new/multiplayer
    ${CMAKE_SOURCE_DIR}/new/game_micro_optimization)
  target_link_libraries(bench_game_micro_amp_capture PRIVATE avp_local_multiplayer)
  target_compile_definitions(bench_game_micro_amp_capture PRIVATE
    AVP_LOCAL_MULTIPLAYER_AMP_BRIDGE=1)
  target_compile_options(bench_game_micro_amp_capture PRIVATE
    -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(bench_game_micro_amp_capture PRIVATE -Wl,--gc-sections)


endif()

# RE26: integrated original-game scope transaction: original weapon/player,
# AMP allocator, doors and collision on two genuinely separate level maps.
# Link-only stubs cover ONLY unrelated hardware/audio output, not mechanics.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_native_context
    tests/test_local_mp_native_context.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/mazescrn.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/amp.c
    ${CMAKE_SOURCE_DIR}/src/game/doors.c
    ${CMAKE_SOURCE_DIR}/src/game/collision.c
    ${CMAKE_SOURCE_DIR}/src/game/hud_message.c
    ${CMAKE_SOURCE_DIR}/src/game/hud.c)
  target_include_directories(test_local_mp_native_context PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_native_context PRIVATE avp_local_multiplayer m)
  target_compile_definitions(test_local_mp_native_context PRIVATE
    AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_AMP_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_DOOR_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_HUDMSG_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_HUDVIEW_BRIDGE=1)
  target_compile_options(test_local_mp_native_context PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_native_context PRIVATE -Wl,--gc-sections)
endif()

# End-to-end integration of the existing session coordinator with the real
# isolated Jaguar-derived actor/world state. Synthetic pixels ONLY in test.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_native_backend
    tests/test_local_mp_native_backend.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c
    ${CMAKE_SOURCE_DIR}/src/game/mazescrn.c
    ${CMAKE_SOURCE_DIR}/src/game/weapons.c
    ${CMAKE_SOURCE_DIR}/src/game/amp.c
    ${CMAKE_SOURCE_DIR}/src/game/doors.c
    ${CMAKE_SOURCE_DIR}/src/game/collision.c
    ${CMAKE_SOURCE_DIR}/src/game/hud_message.c
    ${CMAKE_SOURCE_DIR}/src/game/hud.c)
  target_include_directories(test_local_mp_native_backend PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_native_backend PRIVATE avp_local_multiplayer m)
  target_compile_definitions(test_local_mp_native_backend PRIVATE
    AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_AMP_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_DOOR_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_HUDMSG_BRIDGE=1
    AVP_LOCAL_MULTIPLAYER_HUDVIEW_BRIDGE=1)
  target_compile_options(test_local_mp_native_backend PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_native_backend PRIVATE -Wl,--gc-sections)
endif()

# S62 original PLAYER.S actor/world split, tested against the exact legacy
# UpdatePlayer output (not a rewritten physics model).
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_player_step
    tests/test_local_mp_player_step.c
    ${CMAKE_SOURCE_DIR}/src/game/player.c)
  target_include_directories(test_local_mp_player_step PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(test_local_mp_player_step PRIVATE avp_local_multiplayer m)
  target_compile_definitions(test_local_mp_player_step PRIVATE AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE=1)
  target_compile_options(test_local_mp_player_step PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_player_step PRIVATE -Wl,--gc-sections)
endif()

# RE26 S68: option-scoped Marine shotgun starting loadout (no ROM needed).
add_executable(test_pc_marine_start_shotgun tests/test_pc_marine_start_shotgun.c)
target_include_directories(test_pc_marine_start_shotgun PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_marine_start_shotgun PRIVATE -Wall -Wextra -Werror)
endif()

# Source transition intent ownership: four different original-level exits cannot overwrite.
add_executable(test_local_mp_transition tests/test_local_mp_transition.c local_mp_transition.c)
target_include_directories(test_local_mp_transition PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_transition PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_local_mp_respawn_policy tests/test_local_mp_respawn_policy.c local_mp_respawn_policy.c)
target_include_directories(test_local_mp_respawn_policy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_respawn_policy PRIVATE -Wall -Wextra -Werror)
endif()

# S72: Player-local original-rate death fade and independent restart lifecycle.
add_executable(test_local_mp_life tests/test_local_mp_life.c)
target_include_directories(test_local_mp_life PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_local_mp_life PRIVATE avp_local_multiplayer)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_life PRIVATE -Wall -Wextra -Werror)
endif()

# S76: No synthetic audio: validates source-level membership cue decisions
# and that the exact existing single-player analog lift chord is preserved.
add_executable(test_local_mp_ambient_policy tests/test_local_mp_ambient_policy.c)
target_include_directories(test_local_mp_ambient_policy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_ambient_policy PRIVATE -Wall -Wextra -Werror)
endif()

# Corpse artwork, death-SFX identity and isolated-world persistence contract.
add_executable(test_local_mp_corpses tests/test_local_mp_corpses.c)
target_link_libraries(test_local_mp_corpses PRIVATE avp_local_multiplayer)
target_include_directories(test_local_mp_corpses PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_corpses PRIVATE -Wall -Wextra -Werror)
endif()

# RE27 S80: source-AMP campaign script must not inherit a mixed AI target's
# species; disabled-multiplayer/single-player still uses retail actor type.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_queen_campaign
    tests/test_local_mp_queen_campaign.c
    ${CMAKE_SOURCE_DIR}/src/game/amp.c)
  target_include_directories(test_local_mp_queen_campaign PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_options(test_local_mp_queen_campaign PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_queen_campaign PRIVATE -Wl,--gc-sections)
endif()

# S81: visual-only native Jaguar AMP attack frame selection; no physics changes.
add_executable(test_local_mp_sprite_pose tests/test_local_mp_sprite_pose.c)
target_include_directories(test_local_mp_sprite_pose PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_sprite_pose PRIVATE -Wall -Wextra -Werror)
endif()

# Render-only Marine angle test against the ACTUAL host sprite_choice code.
# GC discards unrelated GPU entry points from this no-ROM source unit.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_marine_sprite_facing
    ${CMAKE_SOURCE_DIR}/tests/test_marine_run_facing.c)
  target_include_directories(test_local_mp_marine_sprite_facing PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_options(test_local_mp_marine_sprite_facing PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_marine_sprite_facing PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_local_mp_marine_sprite_facing PRIVATE m)
endif()

# Real GPU automap simulation/level-history behavior, no ROM redistribution.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_automap_history
    tests/test_local_mp_automap_history.c
    ${CMAKE_SOURCE_DIR}/src/jaguar/avp_gpu/automap.c
    ${CMAKE_SOURCE_DIR}/src/jaguar/avp_gpu/state.c)
  target_include_directories(test_local_mp_automap_history PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_options(test_local_mp_automap_history PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_automap_history PRIVATE -Wl,--gc-sections)
endif()

add_executable(test_local_mp_waveform_proximity tests/test_local_mp_waveform_proximity.c)
target_include_directories(test_local_mp_waveform_proximity PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_waveform_proximity PRIVATE -Wall -Wextra -Werror)
endif()

# Real host projection geometry: default 4:3 path stays original; wide modes
# increase sampled ray columns while keeping the authored Jaguar HUD centered.
add_executable(test_widescreen_geometry tests/test_widescreen_geometry.c)
target_include_directories(test_widescreen_geometry PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_widescreen_geometry PRIVATE -Wall -Wextra -Werror)
endif()

# Native session-wide pause rising-edge contract (no retail ROM required).
add_executable(test_local_mp_pause tests/test_local_mp_pause.c)
target_include_directories(test_local_mp_pause PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_pause PRIVATE -Wall -Wextra -Werror)
endif()

# Exercise the original collectible engine with stale world-table selection.
add_executable(test_local_mp_pickup_levels tests/test_local_mp_pickup_levels.c
    ${CMAKE_SOURCE_DIR}/src/game/collectables.c)
target_include_directories(test_local_mp_pickup_levels PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_pickup_levels PRIVATE -Wall -Wextra -Werror)
endif()

# RE28: four independently selected source-command AI slots.
add_executable(test_local_mp_bot tests/test_local_mp_bot.c)
target_link_libraries(test_local_mp_bot PRIVATE avp_local_multiplayer)
target_include_directories(test_local_mp_bot PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(NOT WIN32)
  target_link_libraries(test_local_mp_bot PRIVATE m)
endif()
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_bot PRIVATE -Wall -Wextra -Werror)
endif()

# RE30 R8: sensory acquisition, 15-m motion tracker and finite contact memory.
add_executable(test_local_mp_bot_awareness tests/test_local_mp_bot_awareness.c)
target_include_directories(test_local_mp_bot_awareness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT WIN32)
  target_link_libraries(test_local_mp_bot_awareness PRIVATE m)
endif()
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_bot_awareness PRIVATE -Wall -Wextra -Werror)
endif()

# R49: screen-space automap contacts use the existing smooth-world pose
# interpolation, never the original 10-Hz Marine scanner coordinates.
add_executable(test_local_mp_automap_dots_smooth
  tests/test_local_mp_automap_dots_smooth.c
  ${CMAKE_SOURCE_DIR}/src/jaguar/avp_gpu/presentation_interp.c)
target_include_directories(test_local_mp_automap_dots_smooth PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_automap_dots_smooth PRIVATE -Wall -Wextra -Werror)
endif()

# RE30 R16: player contacts on the original Marine scanner in all local modes.
add_executable(test_local_mp_tracker_peers tests/test_local_mp_tracker_peers.c)
target_include_directories(test_local_mp_tracker_peers PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(NOT WIN32)
  target_link_libraries(test_local_mp_tracker_peers PRIVATE m)
endif()
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_tracker_peers PRIVATE -Wall -Wextra -Werror)
endif()

# Validate the ACTUAL hosted Jaguar tracker implementation without a ROM.
# Discard unrelated renderer sections to keep the test self-contained.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_tracker_renderer tests/test_local_mp_tracker_renderer.c)
  target_include_directories(test_local_mp_tracker_renderer PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_options(test_local_mp_tracker_renderer PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_tracker_renderer PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_local_mp_tracker_renderer PRIVATE m)
endif()

# RE29 S112: authored vent/lift world-graph routing and jammed storage escape.
add_executable(test_local_mp_bot_levels tests/test_local_mp_bot_levels.c)
target_link_libraries(test_local_mp_bot_levels PRIVATE avp_local_multiplayer m)
target_include_directories(test_local_mp_bot_levels PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_bot_levels PRIVATE -Wall -Wextra -Werror)
endif()
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_bot_retail_graph
    tests/test_local_mp_bot_retail_graph.c
    ${CMAKE_SOURCE_DIR}/src/jaguar/avp_resources/retail_resources.c
    ${CMAKE_SOURCE_DIR}/src/game/doors.c)
  target_link_libraries(test_local_mp_bot_retail_graph PRIVATE avp_local_multiplayer ZLIB::ZLIB m)
  target_include_directories(test_local_mp_bot_retail_graph PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_options(test_local_mp_bot_retail_graph PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_bot_retail_graph PRIVATE -Wl,--gc-sections)
endif()

add_executable(test_local_mp_dm_loot tests/test_local_mp_dm_loot.c)
target_link_libraries(test_local_mp_dm_loot PRIVATE avp_local_multiplayer)
target_include_directories(test_local_mp_dm_loot PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_dm_loot PRIVATE -Wall -Wextra -Werror)
endif()
# R41: 10 Hz deathmatch HUD notifications are strictly per-human viewport,
# while kill attribution still covers all 16 human/bot roster slots.
add_executable(test_local_mp_kill_notice tests/test_local_mp_kill_notice.c)
target_include_directories(test_local_mp_kill_notice PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_kill_notice PRIVATE -Wall -Wextra -Werror)
endif()

# Original Jaguar HUD glyph placement into 320x228 and scaled split views.
# Compile only the renderer's used text functions via section GC. No retail ROM.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_local_mp_kill_notice_raster
    tests/test_local_mp_kill_notice_raster.c
    ${CMAKE_SOURCE_DIR}/src/jaguar/avp_gpu/software_renderer.c)
  target_include_directories(test_local_mp_kill_notice_raster PRIVATE
    ${CMAKE_SOURCE_DIR}/include)
  target_compile_options(test_local_mp_kill_notice_raster PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_local_mp_kill_notice_raster PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_local_mp_kill_notice_raster PRIVATE m)
endif()

# R42: species-dependent ink applies to ALL original HUD_MSG popups;
# their original zero-ink blink phases are never colored over.
add_executable(test_local_mp_popup_ink tests/test_local_mp_popup_ink.c)
target_include_directories(test_local_mp_popup_ink PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_popup_ink PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_local_mp_deathmatch tests/test_local_mp_deathmatch.c)
target_include_directories(test_local_mp_deathmatch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_local_mp_deathmatch PRIVATE avp_local_multiplayer)
if(AVP_STRICT)
  target_compile_options(test_local_mp_deathmatch PRIVATE -Wall -Wextra -Werror)
endif()

# PC-only bot stereo spatialization contract (uses the original contact range).
add_executable(test_local_mp_spatial_audio tests/test_local_mp_spatial_audio.c)
target_include_directories(test_local_mp_spatial_audio PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_local_mp_spatial_audio PRIVATE m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_spatial_audio PRIVATE -Wall -Wextra -Werror)
endif()

# PC opt-in short input enhancements apply to every local player (P1 keeps
# its original path; P2-P4 use the same buffered tick semantics).
add_executable(test_local_mp_enhancement_buffer tests/test_enhancement_buffer.c)
target_include_directories(test_local_mp_enhancement_buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/new/multiplayer)
if(NOT MSVC)
  target_compile_options(test_local_mp_enhancement_buffer PRIVATE -Wall -Wextra -Werror)
endif()

# R70: validate full player floor footprint and fast-mode swept original collision.
add_executable(test_local_mp_move_guard tests/test_local_mp_move_guard.c local_mp_move_guard.c)
target_include_directories(test_local_mp_move_guard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_local_mp_move_guard PRIVATE -Wall -Wextra -Werror)
endif()
