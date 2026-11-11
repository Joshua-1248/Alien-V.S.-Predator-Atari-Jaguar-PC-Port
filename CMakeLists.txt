cmake_minimum_required(VERSION 3.16)
project(avp_jaguar_c_decomp C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(AVP_STRICT "Compile readable C with strict warnings" ON)

add_library(avp_c STATIC
  src/game/main_game.c
  src/game/mazescrn.c
  src/game/maze.c
  src/game/runtime.c
  src/game/files.c
  src/game/sprites.c
  src/game/music.c
  src/game/message.c
  src/game/objects.c
  src/game/font.c
  src/game/computer.c
  src/game/mjp.c
  src/platform/jaguar/blitter.c
  src/platform/jaguar/joypad.c
  src/platform/jaguar/jaguar.c
  src/game/joyed.c
  src/game/player.c
  src/game/gpu_overlays.c
  src/game/collision.c
  src/game/hud_message.c
  src/game/hud.c
  src/game/hud_score.c
  src/game/weapons.c
  src/game/collectables.c
  src/game/amp.c
  src/game/eeprom.c
  src/game/savegame.c
  src/game/levels.c
  src/game/doors.c
  src/game/door_retail_meta.c
  src/game/dev_cheats.c
)
target_include_directories(avp_c PUBLIC include)

# ROM bootstrap support is ordinary 68000 C but is built separately because it
# has a historical allocator ABI and is not part of the RAM-resident game lib.
add_library(avp_unzip_alloc STATIC src/unzip/alloc_reconstructed.c)
add_library(avp_unzip_inflate STATIC src/unzip/inflate_reconstructed.c)
add_library(avp_rom_util STATIC src/unzip/util_reconstructed.c)
add_library(avp_rom_bootstrap STATIC src/unzip/rom_bootstrap.c)
target_include_directories(avp_unzip_alloc PUBLIC include)
target_include_directories(avp_unzip_inflate PUBLIC include)
target_include_directories(avp_rom_util PUBLIC include)
target_include_directories(avp_rom_bootstrap PUBLIC include)
if(WIN32)
  # Historical allocator ABI (malloc(unsigned long)); on LLP64 Windows that
  # differs from the C library prototype.  These oracle libraries are not
  # linked into the game executable, so only GCC's builtin check is relaxed.
  target_compile_options(avp_unzip_alloc PRIVATE -fno-builtin)
  target_compile_options(avp_unzip_inflate PRIVATE -fno-builtin)
  target_compile_options(avp_rom_util PRIVATE -fno-builtin)
  target_compile_options(avp_rom_bootstrap PRIVATE -fno-builtin)
endif()

if(AVP_STRICT)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(avp_c PRIVATE -Wall -Wextra -Werror)
    target_compile_options(avp_unzip_inflate PRIVATE -Wall -Wextra -Werror)
    target_compile_options(avp_rom_util PRIVATE -Wall -Wextra -Werror)
    target_compile_options(avp_rom_bootstrap PRIVATE -Wall -Wextra -Werror)
    # This one intentionally models the 1994 K&R malloc/free ABI.  Keep
    # compiler-specific warning suppressions off Clang so validation remains
    # warning-option clean on both supported host compilers.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_compile_options(avp_unzip_alloc PRIVATE -Wall -Wextra
        -Wno-builtin-declaration-mismatch -Wno-free-nonheap-object)
    else()
      target_compile_options(avp_unzip_alloc PRIVATE -Wall -Wextra)
    endif()
  endif()
endif()


# Host-independent Jaguar compatibility primitives used by the native port.
# Keep these separate from platform/pc so X11/audio/input backends do not own
# Jaguar color or Object Processor semantics.
add_library(avp_jaguar_compat STATIC
  src/jaguar/hw/cry.c
  src/jaguar/hw/object_processor.c
  src/jaguar/hw/framebuffer.c
)
target_include_directories(avp_jaguar_compat PUBLIC include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(avp_jaguar_compat PRIVATE -Wall -Wextra -Werror)
endif()

# RE24-S19: verified native-C original GPU stage driver. This is a pure
# Jaguar-layout bus consumer; no 68000 emulator, no retail GPU code, and no
# change to the existing live renderer. Link into avp_pc for future opt-in
# original GPU rendering once the exact memory bridge is validated.
# RE24 Session 2: hand-written native C port of the original 3D-view GPU
# program (view3d.c), verified 1:1 against the ROM's GPU code by
# tools/jagref/view3d_port_parity.sh.  Linked, not yet the live renderer.
add_library(avp_gpu_original_native STATIC
  src/jaguar/avp_gpu/panels_special_load.c
  src/jaguar/avp_gpu/panels_walk.c
  src/jaguar/avp_gpu/sprite_sets.c
  src/jaguar/avp_gpu/sprite_init.c
  src/jaguar/avp_gpu/view3d.c
  src/jaguar/avp_gpu/jagpeg_load.c
  src/jaguar/avp_gpu/jagpeg_gpu.c
  src/jaguar/avp_gpu/files_dispatch.c
  src/jaguar/avp_gpu/files_jpeg_init.c
  src/jaguar/avp_gpu/files_gzip_vectors.c
  src/jaguar/avp_gpu/panels_cached.c
  src/jaguar/avp_gpu/panels_special_lookup.c
  src/jaguar/avp_gpu/panels_overlay.c
  src/jaguar/avp_gpu/panels_triple_cached.c
  src/jaguar/avp_gpu/panels_triple_rebuild.c
  src/jaguar/avp_gpu/files_jpeg_mode2.c
  src/jaguar/avp_gpu/files_jpeg_mode2_setup.c
  src/jaguar/avp_gpu/files_jpeg_dispatch.c
  src/jaguar/avp_gpu/files_jpeg_native_expand.c
  src/jaguar/avp_gpu/original_view_native.c
  src/jaguar/avp_gpu/original_view_scene.c
  src/jaguar/avp_gpu/original_view_layout.c
)
target_include_directories(avp_gpu_original_native PUBLIC include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(avp_gpu_original_native PRIVATE -Wall -Wextra -Werror)
endif()

# RE24-S10: Atari Jaguar TRM Rev.8 hardware models (GPU/DSP RISC core and
# Blitter).  Pure, host-independent; not linked into the native game.
add_library(avp_jaguar_trm STATIC
  src/jaguar/hw/jrisc.c
  src/jaguar/hw/blitter_trm.c
)
target_include_directories(avp_jaguar_trm PUBLIC include)
target_link_libraries(avp_jaguar_trm PUBLIC avp_jaguar_compat)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(avp_jaguar_trm PRIVATE -Wall -Wextra -Werror)
endif()

# RE24-S10 verification instrument: the original cartridge executed on a
# 68000 core (Musashi, MIT licence, third_party/musashi) with the TRM models.
# R82: the same model can now optionally supply the original title scanout
# in avp_pc (first-boot presentation only; never gameplay simulation).
option(AVP_BUILD_JAG_REFERENCE "Build the Jaguar reference-machine tool (jagref)" ON)
if(AVP_BUILD_JAG_REFERENCE)
  add_library(avp_musashi STATIC
    third_party/musashi/m68kcpu.c
    third_party/musashi/m68kops.c
    third_party/musashi/m68kdasm.c
    third_party/musashi/softfloat/softfloat.c
  )
  target_include_directories(avp_musashi PUBLIC third_party/musashi)
  target_compile_options(avp_musashi PRIVATE -w)
  if(NOT WIN32)
    target_link_libraries(avp_musashi PUBLIC m)
  endif()
  add_library(avp_jag_refmachine STATIC
    src/jaguar/refmachine/jag_machine.c
    src/jaguar/refmachine/gpu_view_access.c)
  target_link_libraries(avp_jag_refmachine PUBLIC avp_jaguar_trm avp_musashi)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(avp_jag_refmachine PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(jagref tools/jagref/jagref_run.c)
  target_link_libraries(jagref PRIVATE avp_jag_refmachine)
  # RE35 R88 read-only original-ROM frame/progress trace for attract parity.
  add_executable(attract_field_trace_r88 tools/jagref/attract_field_trace_r88.c)
  target_link_libraries(attract_field_trace_r88 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(attract_field_trace_r88 PRIVATE -Wall -Wextra -Werror)
  endif()
  # RE36 R102: transaction-level original Jaguar GPU/68000 mailbox observer.
  # Model scheduler coordinates are deliberately NOT hardware timestamps.
  add_executable(attract_mailbox_events_r102 tools/jagref/attract_mailbox_events_r102.c)
  target_link_libraries(attract_mailbox_events_r102 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(attract_mailbox_events_r102 PRIVATE -Wall -Wextra -Werror)
  endif()
  # RE35 R90: observability-only CROSS/OP list and frame fingerprints.
  add_executable(attract_op_field_trace_r90 tools/jagref/attract_op_field_trace_r90.c)
  target_link_libraries(attract_op_field_trace_r90 PRIVATE avp_jag_refmachine)
  # R95: verify Do_Lose/Do_Escap after the original cartridge bootstrap
  # has inflated its RAM-resident 68000 code (never scan compressed ROM bytes).
  add_executable(marine_endings_rom_audit_r95
    tools/jagref/marine_endings_rom_audit_r95.c)
  target_link_libraries(marine_endings_rom_audit_r95 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(marine_endings_rom_audit_r95 PRIVATE -Wall -Wextra -Werror)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(attract_op_field_trace_r90 PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(files_jpeg_install_rom_check tools/jagref/files_jpeg_install_rom_check.c)
  target_link_libraries(files_jpeg_install_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  add_executable(files_jpeg_quant_rom_check tools/jagref/files_jpeg_quant_rom_check.c)
  target_link_libraries(files_jpeg_quant_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  add_executable(files_dispatch_rom_check tools/jagref/files_dispatch_rom_check.c)
  target_link_libraries(files_dispatch_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  # S38: authenticated retail-ROM level JPEG final-image parity (non-live).
  find_package(ZLIB REQUIRED)
  add_executable(files_gzip_qconv_rom_check tools/jagref/files_gzip_qconv_rom_check.c)
  target_link_libraries(files_gzip_qconv_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native ZLIB::ZLIB)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(files_gzip_qconv_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S40: isolated cached-panel walker versus the original retail $F1F6.
  add_executable(panels_cached_rom_check tools/jagref/panels_cached_rom_check.c)
  target_link_libraries(panels_cached_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_cached_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S41: bounded original-ROM notice-buffer and mapped NONE comparison.
  add_executable(panels_notice_rom_check tools/jagref/panels_notice_rom_check.c)
  target_link_libraries(panels_notice_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_notice_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S42: original 68000 special-panel JP80 lookup boundary, no GPU substitution.
  add_executable(test_panels_subpaths_s42 tests/test_panels_subpaths_s42.c)
  target_link_libraries(test_panels_subpaths_s42 PRIVATE avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_panels_subpaths_s42 PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(panels_special_lookup_rom_check tools/jagref/panels_special_lookup_rom_check.c)
  target_link_libraries(panels_special_lookup_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_special_lookup_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S43: original three-pass cache-mismatch Blitter hardware transaction.
  add_executable(panels_triple_rebuild_rom_check tools/jagref/panels_triple_rebuild_rom_check.c)
  target_link_libraries(panels_triple_rebuild_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_triple_rebuild_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(test_panels_rebuild_s43 tests/test_panels_rebuild_s43.c)
  target_link_libraries(test_panels_rebuild_s43 PRIVATE avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_panels_rebuild_s43 PRIVATE -Wall -Wextra -Werror)
  endif()
  # S45: retail $F34E-$F3D8 post-decompression panel Blitter tail.
  # RE25 S46: original JP80 first/cached initialization and GPU launch.
  # S47: test authentic JP80 on the original GPU after naturally loading
  # the level JPEG program and 128KiB QCONV table. Injects a ROM-backed job.
  # S48: native output-level stand-in for the cartridge GZIP service (zlib).
  find_package(ZLIB REQUIRED)
  add_library(avp_gpu_gzip_service STATIC src/jaguar/avp_gpu/files_gzip_service_native.c)
  target_include_directories(avp_gpu_gzip_service PUBLIC include)
  target_link_libraries(avp_gpu_gzip_service PUBLIC ZLIB::ZLIB)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(avp_gpu_gzip_service PRIVATE -Wall -Wextra -Werror)
  endif()
  # S48: tool-only Musashi with the instruction hook (68000 breakpoints).
  add_library(avp_musashi_hook STATIC
    tools/jagref/musashi_hook_cpu.c third_party/musashi/m68kops.c
    third_party/musashi/m68kdasm.c third_party/musashi/softfloat/softfloat.c)
  target_include_directories(avp_musashi_hook PUBLIC third_party/musashi)
  target_compile_options(avp_musashi_hook PRIVATE -w)
  target_compile_definitions(avp_musashi_hook PRIVATE
    M68K_INSTRUCTION_HOOK=M68K_OPT_SPECIFY_HANDLER
    M68K_INSTRUCTION_CALLBACK=avp_m68k_instr_hook)
  if(NOT WIN32)
    target_link_libraries(avp_musashi_hook PUBLIC m)
  endif()
  add_library(avp_jag_refmachine_hook STATIC
    src/jaguar/refmachine/jag_machine.c src/jaguar/refmachine/gpu_view_access.c)
  target_link_libraries(avp_jag_refmachine_hook PUBLIC avp_jaguar_trm avp_musashi_hook)
  # S48: original panel walker $F1F6 in the natural level load vs native.
  add_executable(panels_walk_rom_check tools/jagref/panels_walk_rom_check.c)
  target_link_libraries(panels_walk_rom_check PRIVATE avp_jag_refmachine_hook avp_gpu_original_native avp_gpu_gzip_service)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_walk_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S48: original sprite-set loader $DB3A in the natural level load vs native.
  add_executable(sprite_sets_rom_check tools/jagref/sprite_sets_rom_check.c)
  target_link_libraries(sprite_sets_rom_check PRIVATE avp_jag_refmachine_hook avp_gpu_original_native avp_gpu_gzip_service)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sprite_sets_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S49: natural $DB92 preparation, character assets and $DBFE shared resources.
  add_executable(sprite_init_rom_check tools/jagref/sprite_init_rom_check.c)
  target_link_libraries(sprite_init_rom_check PRIVATE avp_jag_refmachine_hook avp_gpu_original_native avp_gpu_gzip_service)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sprite_init_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S48: continuous original $F2D6 special-panel transaction vs native.
  add_executable(panels_special_load_rom_check tools/jagref/panels_special_load_rom_check.c)
  target_link_libraries(panels_special_load_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native avp_gpu_gzip_service)
  target_include_directories(panels_special_load_rom_check PRIVATE third_party/musashi)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_special_load_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  # S48: original mode-2 RGB->CRY converter on the same 15 genuine resources.
  add_executable(panels_jp80_mode2_cry_rom_check tools/jagref/panels_jp80_mode2_cry_rom_check.c)
  target_link_libraries(panels_jp80_mode2_cry_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_jp80_mode2_cry_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(panels_jp80_mode2_gpu_rom_check tools/jagref/panels_jp80_mode2_gpu_rom_check.c)
  target_link_libraries(panels_jp80_mode2_gpu_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_jp80_mode2_gpu_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(panels_jp80_launch_rom_check tools/jagref/panels_jp80_launch_rom_check.c)
  target_link_libraries(panels_jp80_launch_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_jp80_launch_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(panels_overlay_rom_check tools/jagref/panels_overlay_rom_check.c)
  target_link_libraries(panels_overlay_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_overlay_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(test_panels_overlay_s45 tests/test_panels_overlay_s45.c)
  target_link_libraries(test_panels_overlay_s45 PRIVATE avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_panels_overlay_s45 PRIVATE -Wall -Wextra -Werror)
  endif()
  # S42: 68000 $BF6E mailbox stage, no decoder substitute or side effects.
  # S42: original $F3E6 triple-cache fast-return, no Blitter substitution.
  add_executable(panels_triple_cached_rom_check tools/jagref/panels_triple_cached_rom_check.c)
  target_link_libraries(panels_triple_cached_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panels_triple_cached_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(files_gzip_stage_rom_check tools/jagref/files_gzip_stage_rom_check.c)
  target_link_libraries(files_gzip_stage_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(files_gzip_stage_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(files_gzip_rom_check tools/jagref/files_gzip_rom_check.c)
  target_link_libraries(files_gzip_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(files_gzip_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(files_jpeg_native_rom_check tools/jagref/files_jpeg_native_rom_check.c)
  target_link_libraries(files_jpeg_native_rom_check PRIVATE avp_jag_refmachine avp_gpu_original_native)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(files_jpeg_native_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(files_dispatch_rom_check PRIVATE -Wall -Wextra -Werror)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(jagref PRIVATE -Wall -Wextra -Werror)
  endif()
endif()

option(AVP_BUILD_PC "Build the native PC port on Linux/X11 or Windows/Win32" ON)
if(AVP_BUILD_PC AND ((UNIX AND NOT APPLE) OR WIN32))
  if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
  endif()
  if(NOT WIN32)
    find_package(OpenSSL REQUIRED)  # Windows hashes with the system BCrypt provider
  endif()
  if(MINGW)
    # Prefer the static zlib archive so the Windows executable is standalone.
    set(AVP_SAVED_FIND_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    find_package(ZLIB REQUIRED)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${AVP_SAVED_FIND_SUFFIXES})
  else()
    find_package(ZLIB REQUIRED)
  endif()
  if(NOT WIN32)
    find_package(Threads REQUIRED)
  endif()
  set(AVP_PC_GAME_SOURCES
    src/game/main_game.c
    src/game/mazescrn.c
    src/game/maze.c
    src/game/runtime.c
    src/game/files.c
    src/game/sprites.c
    src/game/music.c
    src/game/message.c
    src/game/objects.c
    src/game/font.c
    src/game/computer.c
    src/game/mjp.c
    src/game/joyed.c
    src/game/player.c
    src/game/gpu_overlays.c
    src/game/collision.c
    src/game/hud_message.c
    src/game/hud.c
    src/game/hud_score.c
    src/game/weapons.c
    src/game/collectables.c
    src/game/amp.c
    src/game/eeprom.c
    src/game/savegame.c
    src/game/levels.c
    src/game/doors.c
    src/game/door_retail_meta.c
    src/game/dev_cheats.c
    src/jaguar/host/blitter_host.c
    src/jaguar/host/joypad_host.c
    src/jaguar/host/jaguar_host.c
  )
  add_library(avp_pc_core STATIC ${AVP_PC_GAME_SOURCES})
  target_include_directories(avp_pc_core PUBLIC include)
target_compile_definitions(avp_pc_core PRIVATE AVP_PC_HOST=1)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(avp_pc_core PRIVATE -Wall -Wextra -Werror)
  endif()

  if(WIN32)
    set(AVP_NATIVE_AUDIO src/platform/pc/pc_audio_winmm.c src/platform/pc/pc_winmm_resample.c)
    set(AVP_NATIVE_VIDEO_HELPERS src/platform/pc/pc_win32_xlib_compat.c src/platform/pc/pc_win32_keymap.c)
    set(AVP_NATIVE_QUICKSTATE src/platform/pc/pc_quickstate.c src/platform/pc/pc_quickstate_disk.c)
  else()
    set(AVP_NATIVE_AUDIO src/platform/pc/pc_audio.c)
    set(AVP_NATIVE_QUICKSTATE src/platform/pc/pc_quickstate.c src/platform/pc/pc_quickstate_disk.c)
  endif()
  add_executable(avp_pc
    ${AVP_NATIVE_VIDEO_HELPERS}
    ${AVP_NATIVE_QUICKSTATE}
    src/jaguar/avp_resources/retail_resources.c
    src/jaguar/host/fullsynth_host.c
    src/jaguar/host/jerry_wave_procedural.c
    src/platform/pc/pc_main.c
    src/platform/pc/pc_original_cheats.c
    src/platform/pc/pc_original_cheat_input.c
    src/platform/pc/pc_runtime.c
    src/platform/pc/pc_rom.c
    src/platform/pc/pc_sha256.c
    src/platform/pc/pc_resources.c
    src/platform/pc/pc_time.c
    src/platform/pc/pc_eeprom.c
    src/platform/pc/pc_video_x11.c
    src/platform/pc/pc_title_native.c
    src/platform/pc/pc_vr_preview.c
    new/map_maker/map_maker.c
    new/map_maker/map_maker_scripts.c
    new/map_maker/map_maker_overhead.c
    new/map_maker/map_maker_highlight.c
    new/map_maker/map_maker_texture_browser.c
    new/map_maker/map_maker_objects.c
    new/map_maker/map_maker_playtest.c
    src/platform/pc/pc_prototype_textures.c
    ${AVP_NATIVE_AUDIO}
    src/platform/pc/pc_audio_layout.c
    src/jaguar/avp_gpu/state.c
    src/jaguar/avp_gpu/automap.c
    src/jaguar/avp_gpu/assets.c
    src/jaguar/avp_gpu/notice_composite.c
    src/jaguar/avp_gpu/software_renderer.c
    src/jaguar/avp_gpu/presentation_interp.c
    src/jaguar/avp_gpu/fp_overlay.c
    src/jaguar/avp_gpu/maze2_column.c
    src/jaguar/avp_gpu/maze2_occlusion.c
    src/jaguar/avp_gpu/tracker_brush.c
    src/jaguar/avp_gpu/frontend_renderer.c
    src/jaguar/avp_gpu/frontend_fade.c
    src/jaguar/avp_gpu/visual_bridge.cpp
    src/jaguar/avp_gpu/jagpeg.cpp
    src/jaguar/avp_gpu/panels.cpp
    src/jaguar/avp_gpu/doors.cpp
    src/jaguar/avp_gpu/cry.cpp
    src/jaguar/avp_gpu/projection.cpp
  )
  target_include_directories(avp_pc PRIVATE new/map_maker)
  # Generic speaker matrix and synthetic audio-mix regression tests.
  # The test assets are generated in memory; no retail audio is bundled.
  add_executable(test_pc_audio_layout
      new/audio/tests/test_audio_layout.c src/platform/pc/pc_audio_layout.c)
  target_include_directories(test_pc_audio_layout PRIVATE include)
  add_executable(test_pc_audio_mix
      tests/test_host_audio.c src/jaguar/host/fullsynth_host.c src/jaguar/host/jerry_wave_procedural.c)
  target_include_directories(test_pc_audio_mix PRIVATE include)
  if(NOT WIN32)
    target_link_libraries(test_pc_audio_mix PRIVATE m pthread)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_audio_layout PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    target_compile_options(test_pc_audio_mix PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # R110: no external JERRY wave-ROM required for synthesized FM instruments.
  add_executable(test_jerry_wave_procedural_r110
      tests/test_jerry_wave_procedural_r110.c
      src/jaguar/host/jerry_wave_procedural.c)
  target_include_directories(test_jerry_wave_procedural_r110 PRIVATE include)
  if(NOT WIN32)
    target_link_libraries(test_jerry_wave_procedural_r110 PRIVATE m)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jerry_wave_procedural_r110 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # RE36 R113: signed Jaguar FONT.S coordinates can describe brushes
  # partially outside the 320x240 CTEXT viewport. Test the actual rasterizer
  # without linking unrelated runtime code from this source-shaped unit.
  if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(test_pc_computer_signed_glyph_clip_r113
        tests/test_pc_computer_signed_glyph_clip_r113.c)
    target_include_directories(test_pc_computer_signed_glyph_clip_r113 PRIVATE include)
    target_compile_options(test_pc_computer_signed_glyph_clip_r113 PRIVATE
        -D_GNU_SOURCE -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(test_pc_computer_signed_glyph_clip_r113 PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_pc_computer_signed_glyph_clip_r113 PRIVATE m)
  endif()
  # R112: the capsule computer must discard prior terminal OP artwork,
  # including its inactive (self-destruct not armed) screen.
  add_executable(test_pc_pod_overlay_reset_r112
      tests/test_pc_pod_overlay_reset_r112.c src/game/computer.c)
  target_include_directories(test_pc_pod_overlay_reset_r112 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_pod_overlay_reset_r112 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # RE36 R114: per-target multiplayer cheat isolation, even on locked targets.
  add_executable(test_local_mp_cheat_targets_r114
      new/multiplayer/tests/test_local_mp_cheat_targets_r114.c)
  target_include_directories(test_local_mp_cheat_targets_r114 PRIVATE new/multiplayer)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_local_mp_cheat_targets_r114 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # R114: multiplayer F2 options survive session creation and
  # player-targeted Reveal Map remains isolated across viewports.
  add_executable(test_local_mp_session_options_r114
      new/multiplayer/tests/test_local_mp_session_options_r114.c
      new/multiplayer/local_mp.c)
  target_include_directories(test_local_mp_session_options_r114 PRIVATE new/multiplayer)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_local_mp_session_options_r114 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # R111: alarm FM feedback uses the generated NOISE phase-zero sample;
  # verify the resulting carrier rate using an original-style synthetic module.
  add_executable(test_jerry_alarm_phase_origin_r111
      tests/test_jerry_alarm_phase_origin_r111.c
      src/jaguar/host/jerry_wave_procedural.c)
  target_include_directories(test_jerry_alarm_phase_origin_r111 PRIVATE include)
  if(NOT WIN32)
    target_link_libraries(test_jerry_alarm_phase_origin_r111 PRIVATE m pthread)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jerry_alarm_phase_origin_r111 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # R114: verify alarm-specific host gain does not alter other sounds.
  add_executable(test_jerry_alarm_volume_r114
      tests/test_jerry_alarm_volume_r114.c
      src/jaguar/host/jerry_wave_procedural.c)
  target_include_directories(test_jerry_alarm_volume_r114 PRIVATE include)
  if(NOT WIN32)
    target_link_libraries(test_jerry_alarm_volume_r114 PRIVATE m pthread)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jerry_alarm_volume_r114 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # PC-only map-format staging/editor tests: no retail ROM required.
  add_executable(test_pc_map_maker new/map_maker/tests/test_map_maker.c new/map_maker/map_maker.c new/map_maker/map_maker_scripts.c new/map_maker/map_maker_overhead.c new/map_maker/map_maker_highlight.c new/map_maker/map_maker_texture_browser.c new/map_maker/map_maker_objects.c)
  target_include_directories(test_pc_map_maker PRIVATE include new/map_maker)
  target_link_libraries(test_pc_map_maker PRIVATE m)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_map_maker PRIVATE -Wall -Wextra -Werror)
  endif()
  add_executable(test_pc_map_maker_amp new/map_maker/tests/test_map_maker_playtest_amp.c
      new/map_maker/map_maker_playtest.c new/map_maker/map_maker_scripts.c
      src/game/door_retail_meta.c)
  # R31: real, unmodified DOORS.S behavioral integration with a relocated
  # Play Test table. The test supplies only the native PC runtime boundary.
  add_executable(test_pc_map_maker_door_flow
      new/map_maker/tests/test_map_maker_door_flow.c
      new/map_maker/map_maker_playtest.c new/map_maker/map_maker_scripts.c
      src/game/door_retail_meta.c src/game/doors.c)
  target_include_directories(test_pc_map_maker_door_flow PRIVATE include new/map_maker)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_map_maker_door_flow PRIVATE -Wall -Wextra -Werror)
  endif()
  target_include_directories(test_pc_map_maker_amp PRIVATE include new/map_maker)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_map_maker_amp PRIVATE -Wall -Wextra -Werror)
  endif()
  # PC-only optional micro-optimized game routines live exclusively in /new.
  # GNU ld --wrap redirects original-game SafePos call sites while preserving
  # its compiled original function as __real_SafePos. No src/game edits.
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND (UNIX OR MINGW))
    target_sources(avp_pc PRIVATE new/game_micro_optimization/game_micro_optimization.c new/game_micro_optimization/amp_free_list.c)
    target_include_directories(avp_pc PRIVATE new/game_micro_optimization)
    target_compile_definitions(avp_pc PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
    target_link_options(avp_pc PRIVATE "-Wl,--wrap=SafePos" "-Wl,--wrap=LineOfSight" "-Wl,--wrap=AMPCollisions")
  else()
    target_sources(avp_pc PRIVATE new/game_micro_optimization/game_micro_optimization.c new/game_micro_optimization/amp_free_list.c)
    target_include_directories(avp_pc PRIVATE new/game_micro_optimization)
  endif()
  if(NOT WIN32)
    target_sources(avp_pc PRIVATE src/platform/pc/pc_idle_inhibit.c)
    add_executable(test_pc_idle_inhibit
      tests/test_pc_idle_inhibit.c src/platform/pc/pc_idle_inhibit.c)
    target_include_directories(test_pc_idle_inhibit PRIVATE src/platform/pc ${X11_INCLUDE_DIR})
    target_link_libraries(test_pc_idle_inhibit PRIVATE ${X11_LIBRARIES} ${CMAKE_DL_LIBS})
    if(AVP_STRICT)
      target_compile_options(test_pc_idle_inhibit PRIVATE -Wall -Wextra -Werror)
    endif()
  endif()
  # Software stereo SBS output for compatible passive viewers and displays.
  add_executable(test_pc_vr_preview tests/test_pc_vr_preview.c
    src/platform/pc/pc_vr_preview.c)
  target_include_directories(test_pc_vr_preview PRIVATE include)
  if(NOT WIN32)
    target_link_libraries(test_pc_vr_preview PRIVATE m)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_vr_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # R78: only the native PC presentation uses measured ROM title fields.
  # Standalone reconstructed Jaguar C and historical tests retain their prior
  # 141-tick surrogate so no common game semantics are silently changed.
  target_compile_definitions(avp_pc PRIVATE AVP_PC_REFERENCE_TITLE_TIMING=1)
  target_include_directories(avp_pc PRIVATE include)
  # Optional PC-only cooperative extension; runtime is OFF until F12 enables it.
  option(AVP_BUILD_LOCAL_MULTIPLAYER "Build the experimental local co-op setup module" ON)
  if(AVP_BUILD_LOCAL_MULTIPLAYER)
    # The Linux/Windows PC executable links avp_pc_core, NOT avp_c.
    # Enable the optional native seams in the actual PC gameplay library;
    # retain them in avp_c for standalone original-game test consumers.
    target_compile_definitions(avp_c PRIVATE AVP_LOCAL_MULTIPLAYER_DOOR_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_AMP_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_HUDMSG_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_HUDVIEW_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE=1)
    target_compile_definitions(avp_pc_core PRIVATE AVP_LOCAL_MULTIPLAYER_DOOR_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_AMP_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_HUDMSG_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_HUDVIEW_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE=1 AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE=1)
    add_subdirectory(new/multiplayer)
    target_link_libraries(avp_pc PRIVATE avp_local_multiplayer)
    target_compile_definitions(avp_pc PRIVATE AVP_LOCAL_MULTIPLAYER=1)
    # PC-only retail title/F2 attract-timer integration (no ROM required).
    if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      add_executable(test_title_f2_attract_guard tests/test_title_f2_attract_guard.c)
      target_include_directories(test_title_f2_attract_guard PRIVATE include)
      target_compile_definitions(test_title_f2_attract_guard PRIVATE AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE=1)
      target_compile_options(test_title_f2_attract_guard PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
      target_link_options(test_title_f2_attract_guard PRIVATE -Wl,--gc-sections)
    endif()
  endif()
  target_link_libraries(avp_pc PRIVATE avp_pc_core avp_jaguar_compat avp_gpu_original_native ZLIB::ZLIB)
  if(AVP_BUILD_JAG_REFERENCE)
    target_link_libraries(avp_pc PRIVATE avp_jag_refmachine)
    target_compile_definitions(avp_pc PRIVATE AVP_PC_NATIVE_TITLE_JAGM=1)
  endif()
  # R91: actual MAIN attract exit must replay the native title before menu.
  if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(test_title_attract_replay_r91 tests/test_title_attract_replay_r91.c)
    target_include_directories(test_title_attract_replay_r91 PRIVATE include)
    target_compile_options(test_title_attract_replay_r91 PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(test_title_attract_replay_r91 PRIVATE -Wl,--gc-sections)
    # RE36 R100: exact MUSIC.o TFX D1 module-volume override must reach
    # FullSynth on the original selected voice, without changing other calls.
    add_executable(test_mjp_tfx_d1_r100
      tests/test_mjp_tfx_d1_r100.c
      src/game/music.c src/game/runtime.c)
    target_include_directories(test_mjp_tfx_d1_r100 PRIVATE include)
    target_compile_definitions(test_mjp_tfx_d1_r100 PRIVATE AVP_PC_HOST=1)
    target_compile_options(test_mjp_tfx_d1_r100 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_executable(test_attract_typ1b_voice_r98
      tests/test_attract_typ1b_voice_r98.c
      src/game/music.c src/game/runtime.c)
    target_include_directories(test_attract_typ1b_voice_r98 PRIVATE include)
    target_compile_definitions(test_attract_typ1b_voice_r98 PRIVATE AVP_PC_HOST=1)
    target_compile_options(test_attract_typ1b_voice_r98 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_executable(test_mjp_text_sound_r91 tests/test_mjp_text_sound_r91.c)
    target_include_directories(test_mjp_text_sound_r91 PRIVATE include)
    target_compile_options(test_mjp_text_sound_r91 PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(test_mjp_text_sound_r91 PRIVATE -Wl,--gc-sections)
    # R95: instruction-backed countdown audio-before-text, $4BEC8 mute,
    # source-derived per-message Y advances and independent Marine failure.
    add_executable(test_mjp_marine_endings_r95 tests/test_mjp_marine_endings_r95.c)
    target_include_directories(test_mjp_marine_endings_r95 PRIVATE include)
    target_compile_options(test_mjp_marine_endings_r95 PRIVATE
      -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(test_mjp_marine_endings_r95 PRIVATE -Wl,--gc-sections)
  endif()
  add_executable(test_title_menu_aperture_r83 tests/test_title_menu_aperture_r83.c
    src/platform/pc/pc_title_native.c
    src/jaguar/avp_gpu/frontend_renderer.c
    src/jaguar/avp_gpu/frontend_fade.c
    src/jaguar/hw/cry.c
    src/jaguar/hw/framebuffer.c)
  target_include_directories(test_title_menu_aperture_r83 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_title_menu_aperture_r83 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # RE35 R89: stage each story glyph/transition on the video field, not
  # as extra X11 repaints from intermediate MJP CPU/audio events.
  add_executable(test_pc_attract_fields_r89 tests/test_pc_attract_fields_r89.c)
  target_include_directories(test_pc_attract_fields_r89 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_attract_fields_r89 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # RE36 R101: source-model MJP CROSS field timeline versus extra host redraws.
  # Regression only: no change to the two-field GPU stage approximation.
  add_executable(test_mjp_cross_field_cadence_r101
    tests/test_mjp_cross_field_cadence_r101.c)
  target_include_directories(test_mjp_cross_field_cadence_r101 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_mjp_cross_field_cadence_r101 PRIVATE
      -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(test_mjp_cross_field_cadence_r101 PRIVATE
      -Wl,--gc-sections)
  endif()
  # RE35 R99: terminal timing uses its own absolute Jaguar video-field
  # deadline. The terminal text routine still waits three original VBlanks.
  add_executable(test_pc_computer_field_clock_r99 tests/test_pc_computer_field_clock_r99.c)
  target_include_directories(test_pc_computer_field_clock_r99 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_computer_field_clock_r99 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # RE35 R88: preserve RGB16 colour across the original MJP CROSS -> OP
  # picture handoff. This tests only the intro/escape, not the title path.
  add_executable(test_intro_rgb16_cross_continuity_r88
    tests/test_intro_rgb16_cross_continuity_r88.c
    src/jaguar/avp_gpu/frontend_renderer.c
    src/jaguar/avp_gpu/frontend_fade.c
    src/jaguar/hw/cry.c
    src/jaguar/hw/framebuffer.c)
  target_include_directories(test_intro_rgb16_cross_continuity_r88 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_intro_rgb16_cross_continuity_r88 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # RE35 R93: verify live green block against the actual user-owned ROM
  # font 235, not the synthetic underscore from the R92 test.
  # The test accepts the ROM on its command line; never bundle ROM bytes.
  if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(test_mjp_live_cursor_rom_r93
      tests/test_mjp_live_cursor_rom_r93.c
      src/jaguar/avp_gpu/frontend_renderer.c
      src/jaguar/avp_gpu/frontend_fade.c
      src/jaguar/hw/cry.c
      src/jaguar/hw/framebuffer.c
      src/jaguar/avp_resources/retail_resources.c)
    target_include_directories(test_mjp_live_cursor_rom_r93 PRIVATE include)
    target_link_libraries(test_mjp_live_cursor_rom_r93 PRIVATE ZLIB::ZLIB)
    target_compile_options(test_mjp_live_cursor_rom_r93 PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(test_mjp_live_cursor_rom_r93 PRIVATE -Wl,--gc-sections)
    if(AVP_STRICT)
      target_compile_options(test_mjp_live_cursor_rom_r93 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
  endif()
  add_executable(test_pc_title_native_rgb tests/test_pc_title_native_rgb.c
    src/platform/pc/pc_title_native.c)
  target_include_directories(test_pc_title_native_rgb PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_title_native_rgb PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  if(AVP_BUILD_JAG_REFERENCE)
    add_executable(test_pc_title_native_replay_r91 tests/test_pc_title_native_replay_r91.c
      src/platform/pc/pc_title_native.c)
    target_link_libraries(test_pc_title_native_replay_r91 PRIVATE avp_jag_refmachine)
    target_include_directories(test_pc_title_native_replay_r91 PRIVATE include)
    target_compile_definitions(test_pc_title_native_replay_r91 PRIVATE AVP_PC_NATIVE_TITLE_JAGM=1)
    if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(test_pc_title_native_replay_r91 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_executable(test_pc_title_native_rom_r82 tests/test_pc_title_native_rom_r82.c
      src/platform/pc/pc_title_native.c)
    target_link_libraries(test_pc_title_native_rom_r82 PRIVATE avp_jag_refmachine)
    target_include_directories(test_pc_title_native_rom_r82 PRIVATE include)
    target_compile_definitions(test_pc_title_native_rom_r82 PRIVATE AVP_PC_NATIVE_TITLE_JAGM=1)
    if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(test_pc_title_native_rom_r82 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
  endif()
  if(NOT WIN32)
    target_link_libraries(avp_pc PRIVATE OpenSSL::Crypto Threads::Threads)
  endif()
  if(WIN32)
    target_link_libraries(avp_pc PRIVATE user32 gdi32 winmm xinput9_1_0 bcrypt)
    set_target_properties(avp_pc PROPERTIES OUTPUT_NAME "AVP_Jaguar_PC")
    if(MINGW)
      # Self-contained executable: only system DLLs are imported (no
      # libgcc/libstdc++/winpthread/zlib runtime DLLs to ship alongside).
      target_link_options(avp_pc PRIVATE -static -static-libgcc -static-libstdc++)
      target_link_options(avp_pc PRIVATE "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/cmake/avp_game_state_pe.ld")
      set_property(TARGET avp_pc APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/avp_game_state_pe.ld")
    endif()
  else()
    target_include_directories(avp_pc PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(avp_pc PRIVATE ${X11_LIBRARIES} ${CMAKE_DL_LIBS} m)
    # Optional XRandR: enumerate/validate actual refresh rates. The game
    # continues to build on X11 hosts without the development package.
    if(X11_Xrandr_INCLUDE_PATH AND X11_Xrandr_LIB)
      target_include_directories(avp_pc PRIVATE ${X11_Xrandr_INCLUDE_PATH})
      target_compile_definitions(avp_pc PRIVATE AVP_HAVE_XRANDR=1)
      target_link_libraries(avp_pc PRIVATE ${X11_Xrandr_LIB})
    endif()
    # Host presentation: MIT-SHM transfer when libXext/XShm is available.
    if(X11_XShm_FOUND AND X11_Xext_LIB)
      target_compile_definitions(avp_pc PRIVATE AVP_HAVE_XSHM=1)
      target_link_libraries(avp_pc PRIVATE ${X11_Xext_LIB})
    endif()
    target_link_options(avp_pc PRIVATE "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/cmake/avp_game_state.ld")
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(avp_pc PRIVATE -Wall -Wextra -Werror)
  endif()
endif()

# M5 static-recompilation track. This is deliberately a separate library while
# coverage grows; the M4.4 readable-C runtime remains available as a reference
# and backend oracle, not the intended long-term authoritative CPU core.
add_library(avp_static_recomp STATIC
  src/recomp/recomp_memory.c
  src/recomp/recomp_dispatch.c
  src/recomp/generated/rom_s_802000.c
)
target_include_directories(avp_static_recomp PUBLIC include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(avp_static_recomp PRIVATE -Wall -Wextra -Werror)
endif()
add_executable(test_static_rom_loader tests/recomp/test_static_rom_loader.c)
target_include_directories(test_static_rom_loader PRIVATE include)
target_link_libraries(test_static_rom_loader PRIVATE avp_static_recomp)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_static_rom_loader PRIVATE -Wall -Wextra -Werror)
endif()

# S92: host-only draw/fog-range policy, independent of the 10-Hz game core.
add_executable(test_host_distance tests/test_host_distance.c)
target_include_directories(test_host_distance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(AVP_STRICT)
  target_compile_options(test_host_distance PRIVATE -Wall -Wextra -Werror)
endif()

# R41: real retail shade-domain smoke test for every PC draw/fog setting.
# These PC-only presentation calculations never affect original Jaguar logic.
add_executable(test_host_distance_render_modes
  tests/test_host_distance_render_modes.cpp
  src/jaguar/avp_gpu/projection.cpp)
target_include_directories(test_host_distance_render_modes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(AVP_STRICT)
  target_compile_options(test_host_distance_render_modes PRIVATE -Wall -Wextra -Werror)
endif()

# RE32 R43: actual floor/ceiling raster with 64x64 synthetic map.  Keep
# sections isolated so the ROM-free test only links the flat rendering pass.
add_executable(test_host_floor_draw_range tests/test_host_floor_draw_range.c)
target_include_directories(test_host_floor_draw_range PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_host_floor_draw_range PRIVATE -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_host_floor_draw_range PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(test_host_floor_draw_range PRIVATE m)

# S93: X11 held-key reconciliation, independent of host event timing.
add_executable(test_pc_x11_held_keys_s93 tests/test_pc_x11_held_keys_s93.c)
target_include_directories(test_pc_x11_held_keys_s93 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_x11_held_keys_s93 PRIVATE -Wall -Wextra -Werror)
endif()

# S105 presentation-only external prototype texture swap regression.
add_executable(test_pc_prototype_textures_s105
  tests/test_pc_prototype_textures_s105.c
  src/platform/pc/pc_prototype_textures.c
  src/jaguar/avp_gpu/notice_composite.c)
target_include_directories(test_pc_prototype_textures_s105 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_prototype_textures_s105 PRIVATE -Wall -Wextra -Werror)
endif()

# RE31 R7: host-only floor/ceiling CRY brightness lookup parity.
add_executable(test_host_flat_shading tests/test_host_flat_shading.c)
target_include_directories(test_host_flat_shading PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_host_flat_shading PRIVATE -Wall -Wextra -Werror)
endif()
if(NOT WIN32)
  add_executable(bench_host_flat_shading tests/bench_host_flat_shading.c)
  target_include_directories(bench_host_flat_shading PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bench_host_flat_shading PRIVATE -Wall -Wextra -Werror)
  endif()
endif()

# R9: compare optional original-game SafePos fast path against actual 68000 C translation.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_game_micro_optimization
    new/game_micro_optimization/tests/test_safe_pos.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(test_game_micro_optimization PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_optimization PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_optimization PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_optimization PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight)
  add_executable(test_game_micro_ai_los
    new/game_micro_optimization/tests/test_enemy_los.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(test_game_micro_ai_los PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_ai_los PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_ai_los PRIVATE -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_ai_los PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight)
  # R11: unchanged native AMP collision fallback for all occupied/edge cells.
  add_executable(test_game_micro_amp_collision
    new/game_micro_optimization/tests/test_amp_collision.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(test_game_micro_amp_collision PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_amp_collision PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_amp_collision PRIVATE -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_amp_collision PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight -Wl,--wrap=AMPCollisions)
  add_executable(bench_game_micro_amp_collision
    new/game_micro_optimization/tests/bench_amp_collision.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(bench_game_micro_amp_collision PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(bench_game_micro_amp_collision PRIVATE include new/game_micro_optimization)
  target_compile_options(bench_game_micro_amp_collision PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(bench_game_micro_amp_collision PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight -Wl,--wrap=AMPCollisions)
  add_executable(bench_game_micro_ai_los
    new/game_micro_optimization/tests/bench_enemy_los.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(bench_game_micro_ai_los PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(bench_game_micro_ai_los PRIVATE include new/game_micro_optimization)
  target_compile_options(bench_game_micro_ai_los PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(bench_game_micro_ai_los PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight)
endif()

# RE31 R13: host-only linear-time AMP allocator free-list validator parity.
# This code is never part of original Jaguar game source.
add_executable(test_game_micro_amp_free_list
  new/game_micro_optimization/tests/test_amp_free_list.c
  new/game_micro_optimization/amp_free_list.c)
target_include_directories(test_game_micro_amp_free_list PRIVATE include new/game_micro_optimization)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_game_micro_amp_free_list PRIVATE -O2 -Wall -Wextra -Werror)
endif()
if(NOT WIN32)
  add_executable(bench_game_micro_amp_free_list
    new/game_micro_optimization/tests/bench_amp_free_list.c
    new/game_micro_optimization/amp_free_list.c)
  target_include_directories(bench_game_micro_amp_free_list PRIVATE include new/game_micro_optimization)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bench_game_micro_amp_free_list PRIVATE -O3 -Wall -Wextra -Werror)
  endif()
endif()

# RE33 R55: source-derived Jaguar keypad decoder; no original game changes.
add_executable(test_pc_original_cheat_input
    tests/test_pc_original_cheat_input.c
    src/platform/pc/pc_original_cheat_input.c)
target_include_directories(test_pc_original_cheat_input PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_original_cheat_input PRIVATE -Wall -Wextra -Werror)
endif()

# RE33 R60: original projectile impact damage and pool-slot collision ownership.
# Test-only Jaguar input/runtime stubs; no retail ROM content in the binary.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_flame_barrel_release_r60
      tests/test_amp_flame_barrel_release_r60.c
      tests/amp_link_stubs.c
      src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_flame_barrel_release_r60 PRIVATE include)
  target_compile_options(test_amp_flame_barrel_release_r60 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_flame_barrel_release_r60 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_flame_barrel_release_r60 PRIVATE m)
endif()

# RE33 R65: PC-only enemy despawn cheat, including Queen (no boss-death hook).
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_clear_enemies_r65
      tests/test_amp_clear_enemies_r65.c tests/amp_link_stubs.c
      src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_clear_enemies_r65 PRIVATE include)
  target_compile_options(test_amp_clear_enemies_r65 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_clear_enemies_r65 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_clear_enemies_r65 PRIVATE m)
endif()

# RE33 R67: no-death removal of native environmental hazards.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_clear_hazards_r67
      tests/test_amp_clear_hazards_r67.c tests/amp_link_stubs.c
      src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_clear_hazards_r67 PRIVATE include)
  target_compile_options(test_amp_clear_hazards_r67 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_clear_hazards_r67 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_clear_hazards_r67 PRIVATE m)
endif()

# RE33 R68: debug refresh drops only the current level saved AMP state.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_refresh_level_r68
      tests/test_amp_refresh_level_r68.c tests/amp_link_stubs.c
      src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_refresh_level_r68 PRIVATE include)
  target_compile_options(test_amp_refresh_level_r68 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_refresh_level_r68 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_refresh_level_r68 PRIVATE m)
endif()

# RE33 R68: Debug menu action and Return To Title input latch.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_f12_debug_refresh_r68 tests/test_f12_debug_refresh_r68.c)
  target_include_directories(test_f12_debug_refresh_r68 PRIVATE include new/multiplayer new/map_maker new/game_micro_optimization)
  target_compile_definitions(test_f12_debug_refresh_r68 PRIVATE AVP_LOCAL_MULTIPLAYER)
  target_compile_options(test_f12_debug_refresh_r68 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_f12_debug_refresh_r68 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_f12_debug_refresh_r68 PRIVATE X11 m ${CMAKE_DL_LIBS})
endif()

# RE35 R86: preserve the original R59 real-settings roundtrip regression.
# This must remain a buildable test as enhancements are added to F12.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_normalize_diagonal_contact_menu_r59
      tests/test_normalize_diagonal_contact_menu_r59.c)
  target_include_directories(test_normalize_diagonal_contact_menu_r59 PRIVATE
      include new/multiplayer new/map_maker new/game_micro_optimization)
  target_compile_definitions(test_normalize_diagonal_contact_menu_r59 PRIVATE
      AVP_LOCAL_MULTIPLAYER=1 AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE=1)
  target_compile_options(test_normalize_diagonal_contact_menu_r59 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_normalize_diagonal_contact_menu_r59 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_normalize_diagonal_contact_menu_r59 PRIVATE X11 m ${CMAKE_DL_LIBS})
endif()

# RE33 R71: test original enemy pursuit distance cutoff and 2x/3x/4x/unlimited.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_enemy_activation_r71
      tests/test_amp_enemy_activation_r71.c tests/amp_link_stubs.c
      src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_enemy_activation_r71 PRIVATE include)
  target_compile_options(test_amp_enemy_activation_r71 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_enemy_activation_r71 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_enemy_activation_r71 PRIVATE m)
endif()

# RE33 R71: F12 enhancement cycling, persistence and invalid-settings fallback.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_f12_enemy_activation_r71 tests/test_f12_enemy_activation_r71.c)
  target_include_directories(test_f12_enemy_activation_r71 PRIVATE include new/multiplayer new/map_maker new/game_micro_optimization)
  target_compile_definitions(test_f12_enemy_activation_r71 PRIVATE AVP_LOCAL_MULTIPLAYER)
  target_compile_options(test_f12_enemy_activation_r71 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_f12_enemy_activation_r71 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_f12_enemy_activation_r71 PRIVATE X11 m)
endif()
