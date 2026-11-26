cmake_minimum_required(VERSION 3.16)
project(avp_jaguar_c_decomp C CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${AVP_PORT_BINARY_ROOT}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${AVP_PORT_BINARY_ROOT}")
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
  new/jaguar/retail_data/door_retail_meta.c
  new/cheats/dev_cheats.c
  new/cheats/damage_gate.c
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
  new/jaguar/hw/cry.c
  new/jaguar/hw/object_processor.c
  new/jaguar/hw/framebuffer.c
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
  new/jaguar/avp_gpu/panels_special_load.c
  new/jaguar/avp_gpu/panels_walk.c
  new/jaguar/avp_gpu/sprite_sets.c
  new/jaguar/avp_gpu/sprite_init.c
  new/jaguar/avp_gpu/view3d.c
  new/jaguar/avp_gpu/jagpeg_load.c
  new/jaguar/avp_gpu/jagpeg_gpu.c
  new/jaguar/avp_gpu/files_dispatch.c
  new/jaguar/avp_gpu/files_jpeg_init.c
  new/jaguar/avp_gpu/files_gzip_vectors.c
  new/jaguar/avp_gpu/panels_cached.c
  new/jaguar/avp_gpu/panels_special_lookup.c
  new/jaguar/avp_gpu/panels_overlay.c
  new/jaguar/avp_gpu/panels_triple_cached.c
  new/jaguar/avp_gpu/panels_triple_rebuild.c
  new/jaguar/avp_gpu/files_jpeg_mode2.c
  new/jaguar/avp_gpu/files_jpeg_mode2_setup.c
  new/jaguar/avp_gpu/files_jpeg_dispatch.c
  new/jaguar/avp_gpu/files_jpeg_native_expand.c
  new/jaguar/avp_gpu/original_view_native.c
  new/jaguar/avp_gpu/original_view_scene.c
  new/jaguar/avp_gpu/original_view_layout.c
)
target_include_directories(avp_gpu_original_native PUBLIC include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(avp_gpu_original_native PRIVATE -Wall -Wextra -Werror)
endif()

# RE24-S10: Atari Jaguar TRM Rev.8 hardware models (GPU/DSP RISC core and
# Blitter).  Pure, host-independent; not linked into the native game.
add_library(avp_jaguar_trm STATIC
  new/jaguar/hw/jrisc.c
  new/jaguar/hw/blitter_trm.c
)
target_include_directories(avp_jaguar_trm PUBLIC include)
target_link_libraries(avp_jaguar_trm PUBLIC avp_jaguar_compat)
# R133: Jaguar TOM address-generator masks and A1-source clipping when
# DSTA2 reverses source/destination. Pure TRM model regressions, no ROM.
add_executable(test_blitter_a2_mask_r133 tests/test_blitter_a2_mask_r133.c)
add_executable(test_blitter_clip_a1_source_r133 tests/test_blitter_clip_a1_source_r133.c)
foreach(blitter_r133_test test_blitter_a2_mask_r133 test_blitter_clip_a1_source_r133)
  target_link_libraries(${blitter_r133_test} PRIVATE avp_jaguar_trm)
  target_include_directories(${blitter_r133_test} PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${blitter_r133_test} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
endforeach()
# RE38 R141: SRCENZ performs a source-Z memory read in the inner loop,
# independently of comparator and write-Z selections (16-bit pixel path).
add_executable(test_blitter_srcenz_cycles_r141 tests/test_blitter_srcenz_cycles_r141.c)
target_link_libraries(test_blitter_srcenz_cycles_r141 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_srcenz_cycles_r141 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_srcenz_cycles_r141 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R145: GPU/DSP SH and SHA handle the full signed 32-bit shift count,
# including INT32_MIN, without C signed overflow.
add_executable(test_jrisc_shift_extremes_r145 tests/test_jrisc_shift_extremes_r145.c)
target_link_libraries(test_jrisc_shift_extremes_r145 PRIVATE avp_jaguar_trm)
target_include_directories(test_jrisc_shift_extremes_r145 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_jrisc_shift_extremes_r145 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R140: prefetch extra source phrase when SRCENX requests it.
add_executable(test_blitter_srcenx_prefetch_r140 tests/test_blitter_srcenx_prefetch_r140.c)
target_link_libraries(test_blitter_srcenx_prefetch_r140 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_srcenx_prefetch_r140 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_srcenx_prefetch_r140 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

add_executable(test_blitter_phrase_carry_r139 tests/test_blitter_phrase_carry_r139.c)
target_link_libraries(test_blitter_phrase_carry_r139 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_phrase_carry_r139 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_phrase_carry_r139 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R138: phrase source/Z prefetch ensures overlapping source and destination
# windows do not change the in-flight source phrase during a write.
add_executable(test_blitter_phrase_latch_r138 tests/test_blitter_phrase_latch_r138.c)
target_link_libraries(test_blitter_phrase_latch_r138 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_phrase_latch_r138 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_phrase_latch_r138 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R137: original TOM B_STOP comparator collision checkpoint, resume and
# abort (non-cycle-exact synchronous reference model).
add_executable(test_blitter_stop_r137 tests/test_blitter_stop_r137.c)
target_link_libraries(test_blitter_stop_r137 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_stop_r137 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_stop_r137 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R136: phrase-mode source alignment requires SRCENX when the first
# destination phrase crosses a source phrase boundary. Test reports missing
# fetch instead of guessing the hardware's stale source-register contents.
add_executable(test_blitter_srcenx_r136 tests/test_blitter_srcenx_r136.c)
target_link_libraries(test_blitter_srcenx_r136 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_srcenx_r136 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_srcenx_r136 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R135: signed, independently saturating CRY channels for ADDDSEL.
add_executable(test_blitter_adddsel_cry_r135 tests/test_blitter_adddsel_cry_r135.c)
target_link_libraries(test_blitter_adddsel_cry_r135 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_adddsel_cry_r135 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_adddsel_cry_r135 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
# RE38 R134: Z source selection must work without the Z comparator.
add_executable(test_blitter_srcz_r134 tests/test_blitter_srcz_r134.c)
target_link_libraries(test_blitter_srcz_r134 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_srcz_r134 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_srcz_r134 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
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
    new/jaguar/refmachine/jag_machine.c
    new/jaguar/refmachine/gpu_view_access.c)
  target_link_libraries(avp_jag_refmachine PUBLIC avp_jaguar_trm avp_musashi)
  # RE38 R147: read-only B_CMD/B_STOP lifecycle and IRQ timestamp audit.
  add_executable(test_jag_blitter_mmio_timeline_r147
    tests/test_jag_blitter_mmio_timeline_r147.c)
  target_link_libraries(test_jag_blitter_mmio_timeline_r147 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_mmio_timeline_r147 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_executable(blitter_irq_trace_r147 tools/jagref/blitter_irq_trace_r147.c)
  target_link_libraries(blitter_irq_trace_r147 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blitter_irq_trace_r147 PRIVATE -Wall -Wextra -Werror)
  endif()
  # RE38 R149: counts Blitter model read/write requests and attributes GPU
  # commands issued inside the Object Processor's GPU wait correctly.
  add_executable(test_jag_blitter_bus_provenance_r149
    tests/test_jag_blitter_bus_provenance_r149.c)
  target_link_libraries(test_jag_blitter_bus_provenance_r149 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_bus_provenance_r149 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_executable(blitter_bus_trace_r149 tools/jagref/blitter_bus_trace_r149.c)
  target_link_libraries(blitter_bus_trace_r149 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blitter_bus_trace_r149 PRIVATE -Wall -Wextra -Werror)
  endif()
  # RE38 R150: source-documented Blitter idle diagnostic bits and read-only
  # B_CMD status-poll provenance (no guessed bus latency).
  add_executable(test_jag_blitter_status_r150
    tests/test_jag_blitter_status_r150.c)
  target_link_libraries(test_jag_blitter_status_r150 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_status_r150 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_executable(blitter_status_trace_r150
    tools/jagref/blitter_status_trace_r150.c)
  target_link_libraries(blitter_status_trace_r150 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blitter_status_trace_r150 PRIVATE -Wall -Wextra -Werror)
  endif()
  # RE38 R151: only Blitter read8/write8 callbacks count as Blitter bus
  # requests. Ordinary 68000/GPU byte stores must not inflate the trace.
  add_executable(test_jag_blitter_request_provenance_r151
    tests/test_jag_blitter_request_provenance_r151.c)
  target_link_libraries(test_jag_blitter_request_provenance_r151 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_request_provenance_r151 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_executable(blitter_request_audit_r151
    tools/jagref/blitter_request_audit_r151.c)
  target_link_libraries(blitter_request_audit_r151 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blitter_request_audit_r151 PRIVATE -Wall -Wextra -Werror)
  endif()
  # RE38 R152: diagnostic Blitter byte-request bins by address region.
  add_executable(test_jag_blitter_regions_r152 tests/test_jag_blitter_regions_r152.c)
  target_link_libraries(test_jag_blitter_regions_r152 PRIVATE avp_jag_refmachine)
  add_executable(blitter_regions_audit_r152 tools/jagref/blitter_regions_audit_r152.c)
  target_link_libraries(blitter_regions_audit_r152 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_regions_r152 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    target_compile_options(blitter_regions_audit_r152 PRIVATE -Wall -Wextra -Werror)
  endif()
  # RE44 R286: reference-only opt-in incremental Blitter lifecycle.  Tests
  # actual staged writes, BUSY, phrase carry, B_STOP and deferred GPU IRQ4.
  # RE44 R287: 68000 timeslice grant while progressive TOM Blitter owns DRAM.
  add_executable(test_jag_blitter_bus_grant_r287
    tests/test_jag_blitter_bus_grant_r287.c)
  target_link_libraries(test_jag_blitter_bus_grant_r287 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_bus_grant_r287 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_executable(test_jag_blitter_async_r286 tests/test_jag_blitter_async_r286.c)
  target_link_libraries(test_jag_blitter_async_r286 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_async_r286 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # RE38 R146: B_CMD/NOGO, collision stop, resume/abort GPU IRQ completion.
  # This exercises real reference-machine MMIO and uses a synthetic cart.
  add_executable(test_jag_blitter_irq_completion_r146
    tests/test_jag_blitter_irq_completion_r146.c)
  target_link_libraries(test_jag_blitter_irq_completion_r146 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blitter_irq_completion_r146 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
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
  add_library(avp_gpu_gzip_service STATIC new/jaguar/avp_gpu/files_gzip_service_native.c)
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
    new/jaguar/refmachine/jag_machine.c new/jaguar/refmachine/gpu_view_access.c)
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
    new/jaguar/retail_data/door_retail_meta.c
    new/cheats/dev_cheats.c
  new/cheats/damage_gate.c
    new/jaguar/host/blitter_host.c
    new/jaguar/host/joypad_host.c
    new/jaguar/host/jaguar_host.c
  )
  add_library(avp_pc_core STATIC ${AVP_PC_GAME_SOURCES})
  target_include_directories(avp_pc_core PUBLIC include)
target_compile_definitions(avp_pc_core PRIVATE AVP_PC_HOST=1 AVP_NEW_INVINCIBILITY_GATE=1)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(avp_pc_core PRIVATE -Wall -Wextra -Werror)
  endif()

  if(WIN32)
    set(AVP_NATIVE_AUDIO new/pc_host/src/pc_audio_winmm.c new/pc_host/src/pc_winmm_resample.c)
    set(AVP_NATIVE_VIDEO_HELPERS new/pc_host/src/pc_win32_xlib_compat.c new/pc_host/src/pc_win32_keymap.c)
    set(AVP_NATIVE_QUICKSTATE new/pc_host/src/pc_quickstate.c new/pc_host/src/pc_quickstate_disk.c)
  else()
    set(AVP_NATIVE_AUDIO new/pc_host/src/pc_audio.c)
    set(AVP_NATIVE_QUICKSTATE new/pc_host/src/pc_quickstate.c new/pc_host/src/pc_quickstate_disk.c)
  endif()
  add_executable(avp_pc
    ${AVP_NATIVE_VIDEO_HELPERS}
    ${AVP_NATIVE_QUICKSTATE}
    new/jaguar/avp_resources/retail_resources.c
    new/jaguar/host/fullsynth_host.c
    new/jaguar/host/jerry_wave_procedural.c
    new/pc_host/src/pc_main.c
    new/pc_host/src/pc_original_cheats.c
    new/pc_host/src/pc_original_cheat_input.c
    new/pc_host/src/pc_runtime.c
    new/pc_host/src/pc_rom.c
    new/pc_host/src/pc_sha256.c
    new/pc_host/src/pc_resources.c
    new/pc_host/src/pc_time.c
    new/pc_host/src/pc_eeprom.c
    new/pc_host/src/pc_video_x11.c
    new/pc_host/src/pc_vulkan_probe.c
    new/jaguar/avp_gpu/world_scene_packet.c
    new/jaguar/avp_gpu/world_scene_gpu_wire.c
    new/jaguar/avp_gpu/world_texture_transfer.c
    new/pc_host/src/pc_vignette.c
    new/pc_host/src/pc_title_native.c
    new/pc_host/src/pc_vr_preview.c
    new/map_maker/map_maker.c
    new/map_maker/map_maker_scripts.c
    new/map_maker/map_maker_overhead.c
    new/map_maker/map_maker_highlight.c
    new/map_maker/map_maker_texture_browser.c
    new/map_maker/map_maker_objects.c
    new/map_maker/map_maker_playtest.c
    new/pc_host/src/pc_prototype_textures.c
    new/pc_host/src/pc_hi_res_textures.c
    new/pc_host/src/pc_hi_res_sprites.c
    ${AVP_NATIVE_AUDIO}
    new/audio/pc_sound_channels.c
    new/pc_host/src/pc_audio_layout.c
    new/jaguar/avp_gpu/state.c
    new/jaguar/avp_gpu/automap.c
    new/jaguar/avp_gpu/assets.c
    new/jaguar/avp_gpu/notice_composite.c
    new/jaguar/avp_gpu/software_renderer.c
    new/jaguar/avp_gpu/texture_filter.c
    new/jaguar/avp_gpu/thermal_vision.c
    new/jaguar/avp_gpu/presentation_interp.c
    new/jaguar/avp_gpu/fp_overlay.c
    new/jaguar/avp_gpu/maze2_column.c
    new/jaguar/avp_gpu/maze2_occlusion.c
    new/jaguar/avp_gpu/tracker_brush.c
    new/jaguar/avp_gpu/frontend_renderer.c
    new/jaguar/avp_gpu/frontend_fade.c
    new/jaguar/avp_gpu/visual_bridge.cpp
    new/jaguar/avp_gpu/jagpeg.cpp
    new/jaguar/avp_gpu/panels.cpp
    new/jaguar/avp_gpu/doors.cpp
    new/jaguar/avp_gpu/cry.cpp
    new/jaguar/avp_gpu/projection.cpp
  )
  target_include_directories(avp_pc PRIVATE new/map_maker)
  # Generic speaker matrix and synthetic audio-mix regression tests.
  # The test assets are generated in memory; no retail audio is bundled.
  add_executable(test_pc_audio_layout
      new/audio/tests/test_audio_layout.c new/pc_host/src/pc_audio_layout.c)
  target_include_directories(test_pc_audio_layout PRIVATE include)
  add_executable(test_pc_audio_mix
      tests/test_host_audio.c new/jaguar/host/fullsynth_host.c new/jaguar/host/jerry_wave_procedural.c)
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
      new/jaguar/host/jerry_wave_procedural.c)
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
      new/jaguar/host/jerry_wave_procedural.c)
  target_include_directories(test_jerry_alarm_phase_origin_r111 PRIVATE include)
  if(NOT WIN32)
    target_link_libraries(test_jerry_alarm_phase_origin_r111 PRIVATE m pthread)
  endif()
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jerry_alarm_phase_origin_r111 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  # R114: verify alarm-specific host gain does not alter other sounds.
  add_executable(test_fullsynth_mp_spatial_r126
      tests/test_fullsynth_mp_spatial_r126.c
      new/jaguar/host/jerry_wave_procedural.c)
  target_include_directories(test_fullsynth_mp_spatial_r126 PRIVATE include new/multiplayer)
  target_compile_definitions(test_fullsynth_mp_spatial_r126 PRIVATE AVP_LOCAL_MULTIPLAYER=1)
  target_link_libraries(test_fullsynth_mp_spatial_r126 PRIVATE m pthread)
  if(AVP_STRICT)
    target_compile_options(test_fullsynth_mp_spatial_r126 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_executable(test_jerry_alarm_volume_r114
      tests/test_jerry_alarm_volume_r114.c
      new/jaguar/host/jerry_wave_procedural.c)
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
      new/jaguar/retail_data/door_retail_meta.c)
  # R31: real, unmodified DOORS.S behavioral integration with a relocated
  # Play Test table. The test supplies only the native PC runtime boundary.
  add_executable(test_pc_map_maker_door_flow
      new/map_maker/tests/test_map_maker_door_flow.c
      new/map_maker/map_maker_playtest.c new/map_maker/map_maker_scripts.c
      new/jaguar/retail_data/door_retail_meta.c src/game/doors.c)
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
    target_link_options(avp_pc PRIVATE "-Wl,--wrap=SafePos" "-Wl,--wrap=LineOfSight" "-Wl,--wrap=AMPCollisions" "-Wl,--wrap=Vector" "-Wl,--wrap=NMoves" "-Wl,--wrap=sin_d0" "-Wl,--wrap=cos_d0" -Wl,--wrap=avp_collision_bind_maze)
  else()
    target_sources(avp_pc PRIVATE new/game_micro_optimization/game_micro_optimization.c new/game_micro_optimization/amp_free_list.c)
    target_include_directories(avp_pc PRIVATE new/game_micro_optimization)
  endif()
  if(NOT WIN32)
    target_sources(avp_pc PRIVATE new/pc_host/src/pc_idle_inhibit.c)
    add_executable(test_pc_idle_inhibit
      tests/test_pc_idle_inhibit.c new/pc_host/src/pc_idle_inhibit.c)
    target_include_directories(test_pc_idle_inhibit PRIVATE new/pc_host/src ${X11_INCLUDE_DIR})
    target_link_libraries(test_pc_idle_inhibit PRIVATE ${X11_LIBRARIES} ${CMAKE_DL_LIBS})
    if(AVP_STRICT)
      target_compile_options(test_pc_idle_inhibit PRIVATE -Wall -Wextra -Werror)
    endif()
  endif()
  # Software stereo SBS output for compatible passive viewers and displays.
  add_executable(test_pc_vr_preview tests/test_pc_vr_preview.c
    new/pc_host/src/pc_vr_preview.c)
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
    add_subdirectory(new/multiplayer "${AVP_PORT_BINARY_ROOT}/new/multiplayer")
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
    new/pc_host/src/pc_title_native.c
    new/jaguar/avp_gpu/frontend_renderer.c
    new/jaguar/avp_gpu/frontend_fade.c
    new/jaguar/hw/cry.c
    new/jaguar/hw/framebuffer.c)
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
    new/jaguar/avp_gpu/frontend_renderer.c
    new/jaguar/avp_gpu/frontend_fade.c
    new/jaguar/hw/cry.c
    new/jaguar/hw/framebuffer.c)
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
      new/jaguar/avp_gpu/frontend_renderer.c
      new/jaguar/avp_gpu/frontend_fade.c
      new/jaguar/hw/cry.c
      new/jaguar/hw/framebuffer.c
      new/jaguar/avp_resources/retail_resources.c)
    target_include_directories(test_mjp_live_cursor_rom_r93 PRIVATE include)
    target_link_libraries(test_mjp_live_cursor_rom_r93 PRIVATE ZLIB::ZLIB)
    target_compile_options(test_mjp_live_cursor_rom_r93 PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(test_mjp_live_cursor_rom_r93 PRIVATE -Wl,--gc-sections)
    if(AVP_STRICT)
      target_compile_options(test_mjp_live_cursor_rom_r93 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
  endif()
  add_executable(test_pc_title_native_rgb tests/test_pc_title_native_rgb.c
    new/pc_host/src/pc_title_native.c)
  target_include_directories(test_pc_title_native_rgb PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_pc_title_native_rgb PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  if(AVP_BUILD_JAG_REFERENCE)
    add_executable(test_pc_title_native_replay_r91 tests/test_pc_title_native_replay_r91.c
      new/pc_host/src/pc_title_native.c)
    target_link_libraries(test_pc_title_native_replay_r91 PRIVATE avp_jag_refmachine)
    target_include_directories(test_pc_title_native_replay_r91 PRIVATE include)
    target_compile_definitions(test_pc_title_native_replay_r91 PRIVATE AVP_PC_NATIVE_TITLE_JAGM=1)
    if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(test_pc_title_native_replay_r91 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_executable(test_pc_title_native_rom_r82 tests/test_pc_title_native_rom_r82.c
      new/pc_host/src/pc_title_native.c)
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
    elseif(NOT X11_XShm_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/include/pc/pc_xshm_compat.h")
      # Build host MIT-SHM support even on minimal Linux toolchains that ship
      # libXext.so.6 but omit the optional development headers/symlink.
      # Resolve the runtime ELF using the compiler's normal library paths;
      # never hardcode an architecture or let this affect Windows/other hosts.
      execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=libXext.so.6
        OUTPUT_VARIABLE AVP_XEXT_SONAME_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(IS_ABSOLUTE "${AVP_XEXT_SONAME_PATH}" AND EXISTS "${AVP_XEXT_SONAME_PATH}")
        target_compile_definitions(avp_pc PRIVATE AVP_HAVE_XSHM=1 AVP_XSHM_COMPAT_DECLS=1)
        target_link_libraries(avp_pc PRIVATE "${AVP_XEXT_SONAME_PATH}")
      endif()
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
  new/recomp/recomp_memory.c
  new/recomp/recomp_dispatch.c
  new/recomp/generated/rom_s_802000.c
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
  new/jaguar/avp_gpu/projection.cpp)
target_include_directories(test_host_distance_render_modes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(AVP_STRICT)
  target_compile_options(test_host_distance_render_modes PRIVATE -Wall -Wextra -Werror)
endif()

# RE32 R43: actual floor/ceiling raster with 64x64 synthetic map.  Keep
# sections isolated so the ROM-free test only links the flat rendering pass.
add_executable(test_host_floor_draw_range tests/test_host_floor_draw_range.c
  new/jaguar/avp_gpu/texture_filter.c)
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
  new/pc_host/src/pc_prototype_textures.c
  new/jaguar/avp_gpu/notice_composite.c)
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
  target_link_options(test_game_micro_optimization PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight -Wl,--wrap=avp_collision_bind_maze)
  add_executable(test_game_micro_ai_los
    new/game_micro_optimization/tests/test_enemy_los.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(test_game_micro_ai_los PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_ai_los PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_ai_los PRIVATE -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_ai_los PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight -Wl,--wrap=avp_collision_bind_maze)
  # R11: unchanged native AMP collision fallback for all occupied/edge cells.
  add_executable(test_game_micro_amp_collision
    new/game_micro_optimization/tests/test_amp_collision.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(test_game_micro_amp_collision PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_amp_collision PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_amp_collision PRIVATE -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_amp_collision PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight -Wl,--wrap=AMPCollisions -Wl,--wrap=avp_collision_bind_maze)
  add_executable(bench_game_micro_amp_collision
    new/game_micro_optimization/tests/bench_amp_collision.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(bench_game_micro_amp_collision PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(bench_game_micro_amp_collision PRIVATE include new/game_micro_optimization)
  target_compile_options(bench_game_micro_amp_collision PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(bench_game_micro_amp_collision PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight -Wl,--wrap=AMPCollisions -Wl,--wrap=avp_collision_bind_maze)
  add_executable(bench_game_micro_ai_los
    new/game_micro_optimization/tests/bench_enemy_los.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(bench_game_micro_ai_los PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(bench_game_micro_ai_los PRIVATE include new/game_micro_optimization)
  target_compile_options(bench_game_micro_ai_los PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(bench_game_micro_ai_los PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=LineOfSight -Wl,--wrap=avp_collision_bind_maze)
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
    new/pc_host/src/pc_original_cheat_input.c)
target_include_directories(test_pc_original_cheat_input PRIVATE ${PROJECT_SOURCE_DIR}/include)
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

# RE38 R154: original-only 68000 ADD.L score wrap and signed clamp parity.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_score_wrap_r154
      tests/test_amp_score_wrap_r154.c tests/amp_link_stubs.c
      tests/amp_score_link_stubs_r252.c src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_score_wrap_r154 PRIVATE include)
  target_compile_options(test_amp_score_wrap_r154 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_score_wrap_r154 PRIVATE -Wl,--gc-sections)
  # R252: Keep the original 10,560 score-wrap cases registered under CTest.
  add_test(NAME amp_score_wrap_r154 COMMAND test_amp_score_wrap_r154)
endif()

# RE39 R177: opt-in Predator campaign cloaked-kill honor policies, scored by
# the real Jaguar do_score and isolated from multiplayer / other species.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_cloaked_honor_r177
      tests/test_predator_cloaked_honor_r177.c tests/amp_link_stubs.c
      tests/amp_score_link_stubs_r252.c src/game/amp.c src/game/runtime.c)
  target_include_directories(test_predator_cloaked_honor_r177 PRIVATE include)
  target_compile_options(test_predator_cloaked_honor_r177 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_predator_cloaked_honor_r177 PRIVATE -Wl,--gc-sections)
  add_test(NAME predator_cloaked_honor_r177 COMMAND test_predator_cloaked_honor_r177)
endif()

# RE38 R155: original AMP.S weapon-hit persistence of preexisting INVHIT.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_weapon_invhit_r155
      tests/test_amp_weapon_invhit_r155.c tests/amp_link_stubs.c
      src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_weapon_invhit_r155 PRIVATE include)
  target_compile_options(test_amp_weapon_invhit_r155 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_weapon_invhit_r155 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_weapon_invhit_r155 PRIVATE m)
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
      new/cheats/tests/test_clear_hazards_r461.c tests/amp_link_stubs.c
      src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_clear_hazards_r67 PRIVATE include)
  target_compile_options(test_amp_clear_hazards_r67 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_clear_hazards_r67 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_clear_hazards_r67 PRIVATE m)
  add_test(NAME clear_hazards_r461 COMMAND test_amp_clear_hazards_r67)
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
  add_executable(test_f12_debug_refresh_r68 new/pc_host/tests/test_f12_debug_refresh_r68.c)
  target_include_directories(test_f12_debug_refresh_r68 PRIVATE include new/multiplayer new/map_maker new/game_micro_optimization)
  target_compile_definitions(test_f12_debug_refresh_r68 PRIVATE AVP_LOCAL_MULTIPLAYER)
  target_compile_options(test_f12_debug_refresh_r68 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_f12_debug_refresh_r68 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_f12_debug_refresh_r68 PRIVATE X11 m ${CMAKE_DL_LIBS})
  # RE47 R342 audit: these source-inclusion menu regressions pull the
  # current high-resolution loader, filter cache and Vulkan selection entry
  # points from pc_video_x11.c. Link their real implementations, not test
  # stubs, so the historical menu assertions keep testing real behavior.
  target_sources(test_f12_debug_refresh_r68 PRIVATE
    new/pc_host/src/pc_hi_res_textures.c
    new/pc_host/src/pc_hi_res_sprites.c
    new/pc_host/src/pc_vulkan_probe.c
    new/jaguar/avp_gpu/texture_filter.c
    new/jaguar/avp_gpu/visual_bridge.cpp
    new/jaguar/hw/cry.c)
  set_target_properties(test_f12_debug_refresh_r68 PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(test_f12_debug_refresh_r68 PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})
  add_test(NAME f12_debug_refresh_r68 COMMAND test_f12_debug_refresh_r68)
endif()

# RE35 R86: preserve the original R59 real-settings roundtrip regression.
# This must remain a buildable test as enhancements are added to F12.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_normalize_diagonal_contact_menu_r59
      new/pc_host/tests/test_normalize_diagonal_contact_menu_r59.c)
  target_include_directories(test_normalize_diagonal_contact_menu_r59 PRIVATE
      include new/multiplayer new/map_maker new/game_micro_optimization)
  target_compile_definitions(test_normalize_diagonal_contact_menu_r59 PRIVATE
      AVP_LOCAL_MULTIPLAYER=1 AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE=1)
  target_compile_options(test_normalize_diagonal_contact_menu_r59 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_normalize_diagonal_contact_menu_r59 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_normalize_diagonal_contact_menu_r59 PRIVATE X11 m ${CMAKE_DL_LIBS})
  # RE47 R342 audit: these source-inclusion menu regressions pull the
  # current high-resolution loader, filter cache and Vulkan selection entry
  # points from pc_video_x11.c. Link their real implementations, not test
  # stubs, so the historical menu assertions keep testing real behavior.
  target_sources(test_normalize_diagonal_contact_menu_r59 PRIVATE
    new/pc_host/src/pc_hi_res_textures.c
    new/pc_host/src/pc_hi_res_sprites.c
    new/pc_host/src/pc_vulkan_probe.c
    new/jaguar/avp_gpu/texture_filter.c
    new/jaguar/avp_gpu/visual_bridge.cpp
    new/jaguar/hw/cry.c)
  set_target_properties(test_normalize_diagonal_contact_menu_r59 PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(test_normalize_diagonal_contact_menu_r59 PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})
  add_test(NAME normalize_diagonal_contact_menu_r59 COMMAND test_normalize_diagonal_contact_menu_r59)
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
  # RE47 R342 audit: these source-inclusion menu regressions pull the
  # current high-resolution loader, filter cache and Vulkan selection entry
  # points from pc_video_x11.c. Link their real implementations, not test
  # stubs, so the historical menu assertions keep testing real behavior.
  target_sources(test_f12_enemy_activation_r71 PRIVATE
    new/pc_host/src/pc_hi_res_textures.c
    new/pc_host/src/pc_hi_res_sprites.c
    new/pc_host/src/pc_vulkan_probe.c
    new/jaguar/avp_gpu/texture_filter.c
    new/jaguar/avp_gpu/visual_bridge.cpp
    new/jaguar/hw/cry.c)
  set_target_properties(test_f12_enemy_activation_r71 PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(test_f12_enemy_activation_r71 PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})
  add_test(NAME f12_enemy_activation_r71 COMMAND test_f12_enemy_activation_r71)
endif()

# RE37 R127: SELECT briefing's fresh A/B/C skip must not create an extra
# confirmation; the normal post-briefing Fire prompt remains available.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_pc_select_brief_skip_r127
    tests/test_pc_select_brief_skip_r127.c
    src/game/mjp.c src/game/runtime.c)
  target_include_directories(test_pc_select_brief_skip_r127 PRIVATE include)
  target_compile_options(test_pc_select_brief_skip_r127 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_select_brief_skip_r127 PRIVATE -Wl,--gc-sections)
endif()

if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_pc_select_renderer_skip_r127
    tests/test_pc_select_renderer_skip_r127.c
    new/jaguar/avp_gpu/frontend_renderer.c)
  target_include_directories(test_pc_select_renderer_skip_r127 PRIVATE include)
  target_compile_options(test_pc_select_renderer_skip_r127 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_select_renderer_skip_r127 PRIVATE -Wl,--gc-sections)
endif()

if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_pc_select_wheel_r127
    tests/test_pc_select_wheel_r127.c
    src/game/mjp.c src/game/runtime.c)
  target_include_directories(test_pc_select_wheel_r127 PRIVATE include)
  target_compile_options(test_pc_select_wheel_r127 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_select_wheel_r127 PRIVATE -Wl,--gc-sections)
endif()

# RE38 R142: 8-bit stationary character mask uses the comparator's bit
# counter (not the low bit of an entire 8-bit source sample).
add_executable(test_blitter_bcomp_fixed_mask_r142 tests/test_blitter_bcomp_fixed_mask_r142.c)
target_link_libraries(test_blitter_bcomp_fixed_mask_r142 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_bcomp_fixed_mask_r142 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_bcomp_fixed_mask_r142 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R143: destination Z memory reads are independently enabled by DSTENZ.
add_executable(test_blitter_dstz_cycles_r143 tests/test_blitter_dstz_cycles_r143.c)
target_link_libraries(test_blitter_dstz_cycles_r143 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_dstz_cycles_r143 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_dstz_cycles_r143 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R144: complete source-color phrase fetch must precede its Z phrase
# fetch, including the SRCENX prefetch and masked-A2 fallback path.
add_executable(test_blitter_color_z_phase_r144 tests/test_blitter_color_z_phase_r144.c)
target_link_libraries(test_blitter_color_z_phase_r144 PRIVATE avp_jaguar_trm)
target_include_directories(test_blitter_color_z_phase_r144 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_blitter_color_z_phase_r144 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()

# RE38 R153: preserve exact local GPU/DSP RAM byte semantics while reducing
# the reference Blitter's unnecessary 32-bit RMW around GPU local RAM writes.
if(TARGET avp_jag_refmachine)
  add_executable(test_jag_blit_local_ram_r153 tests/test_jag_blit_local_ram_r153.c)
  target_link_libraries(test_jag_blit_local_ram_r153 PRIVATE avp_jag_refmachine)
  add_executable(blitter_state_digest_r153 tools/jagref/blitter_state_digest_r153.c)
  target_link_libraries(blitter_state_digest_r153 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_blit_local_ram_r153 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    target_compile_options(blitter_state_digest_r153 PRIVATE -Wall -Wextra -Werror)
  endif()
endif()

# RE38 R156: original-only placement-grid audit. The same source tool can be
# compiled against the independent Jaguar C reconstruction's levels.c for a
# 15-maze differential using a user-provided retail AVP.jag. Test fixtures do
# not require or distribute ROM data or alter gameplay/bot behavior.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(amp_grid_retail_audit_r156
      tools/original/amp_grid_retail_audit_r156.c
      src/game/levels.c new/jaguar/avp_resources/retail_resources.c)
  target_include_directories(amp_grid_retail_audit_r156 PRIVATE include)
  target_compile_options(amp_grid_retail_audit_r156 PRIVATE
      -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(amp_grid_retail_audit_r156 PRIVATE -Wl,--gc-sections)
  target_link_libraries(amp_grid_retail_audit_r156 PRIVATE ZLIB::ZLIB)
  enable_testing()
  add_test(NAME amp_grid_retail_selftest_r156
      COMMAND amp_grid_retail_audit_r156 --selftest)
endif()

# R160 PC-only Predator Thermal enhancement, original game paths excluded.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_vision_modes_r161
      tests/test_predator_vision_modes_r161.c new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_vision_modes_r161 PRIVATE include)
  target_compile_options(test_predator_vision_modes_r161 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_vision_modes_r161 PRIVATE m)
  add_test(NAME predator_vision_modes_r161 COMMAND test_predator_vision_modes_r161)

  add_executable(test_predator_thermal_r160
      tests/test_predator_thermal_r160.c new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_thermal_r160 PRIVATE include)
  target_compile_options(test_predator_thermal_r160 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_thermal_r160 PRIVATE m)
  add_test(NAME predator_thermal_r160 COMMAND test_predator_thermal_r160)
endif()

if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_f12_predator_thermal_r160 tests/test_f12_predator_thermal_r160.c)
  target_include_directories(test_f12_predator_thermal_r160 PRIVATE
      include new/multiplayer new/map_maker new/game_micro_optimization)
  target_compile_definitions(test_f12_predator_thermal_r160 PRIVATE AVP_LOCAL_MULTIPLAYER)
  target_compile_options(test_f12_predator_thermal_r160 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_f12_predator_thermal_r160 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_f12_predator_thermal_r160 PRIVATE X11 m)
  # RE47 R342: R160 menu test now reaches every maintained menu branch.
  # Reuse the real high-resolution loader, filter and GPU-selection helpers.
  target_sources(test_f12_predator_thermal_r160 PRIVATE
    new/pc_host/src/pc_hi_res_textures.c
    new/pc_host/src/pc_hi_res_sprites.c
    new/pc_host/src/pc_vulkan_probe.c
    new/jaguar/avp_gpu/texture_filter.c
    new/jaguar/avp_gpu/visual_bridge.cpp
    new/jaguar/hw/cry.c)
  set_target_properties(test_f12_predator_thermal_r160 PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(test_f12_predator_thermal_r160 PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})
  add_test(NAME f12_predator_thermal_r160 COMMAND test_f12_predator_thermal_r160)
endif()
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_thermal_sprite_r160
      tests/test_predator_thermal_sprite_r160.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_thermal_sprite_r160 PRIVATE include)
  target_compile_options(test_predator_thermal_sprite_r160 PRIVATE
      -ffunction-sections -fdata-sections -Wno-unused-function -UNDEBUG)
  target_link_options(test_predator_thermal_sprite_r160 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_predator_thermal_sprite_r160 PRIVATE m)
  # RE47 R342 audit: source-included renderer calls real filtering helpers.
  target_sources(test_predator_thermal_sprite_r160 PRIVATE
    new/jaguar/avp_gpu/texture_filter.c
    tests/test_predator_thermal_cry_qconv_link_r342.c)
  add_test(NAME predator_thermal_sprite_r160 COMMAND test_predator_thermal_sprite_r160)
endif()

# RE39 R162: visible-screen filter identities, not inferred table-position names.
# This intentionally catches the R161 wrong-mode enhancement regression.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_filter_bindings_r162
      tests/test_predator_filter_bindings_r162.c
      new/jaguar/avp_gpu/thermal_vision.c
      new/jaguar/hw/cry.c)
  target_include_directories(test_predator_filter_bindings_r162 PRIVATE include)
  target_compile_options(test_predator_filter_bindings_r162 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_filter_bindings_r162 PRIVATE m)
  add_test(NAME predator_filter_bindings_r162
      COMMAND test_predator_filter_bindings_r162)
endif()

# RE39 R163: movie-art-inspired close-red Thermal palette. Regression test
# catches the R162 phenomenon where moving away turned warm sprites red.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_thermal_proximity_r163
      tests/test_predator_thermal_proximity_r163.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_thermal_proximity_r163 PRIVATE include)
  target_compile_options(test_predator_thermal_proximity_r163 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_thermal_proximity_r163 PRIVATE m)
  add_test(NAME predator_thermal_proximity_r163
      COMMAND test_predator_thermal_proximity_r163)
endif()

# RE39 R164: cinematic Infrared overlays only living AAF sprites, using
# the original RGB16 Infrared register pair and a separate opt-in toggle.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_infrared_r164
      tests/test_predator_infrared_r164.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_infrared_r164 PRIVATE include)
  target_compile_options(test_predator_infrared_r164 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_infrared_r164 PRIVATE m)
  add_test(NAME predator_infrared_r164 COMMAND test_predator_infrared_r164)
endif()

# RE39 R165: FLIR-calibrated Infrared midtones; preserve approved Thermal/EM.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_infrared_flir_palette_r165
      tests/test_predator_infrared_flir_palette_r165.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_infrared_flir_palette_r165 PRIVATE include)
  target_compile_options(test_predator_infrared_flir_palette_r165 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_infrared_flir_palette_r165 PRIVATE m)
  add_test(NAME predator_infrared_flir_palette_r165
      COMMAND test_predator_infrared_flir_palette_r165)
endif()

# RE39 R166: independent, optional scenery-only multispectrum enhancement.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_background_r166
      tests/test_predator_background_r166.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_background_r166 PRIVATE include)
  target_compile_options(test_predator_background_r166 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_background_r166 PRIVATE m)
  add_test(NAME predator_background_r166 COMMAND test_predator_background_r166)
endif()

# RE39 R170: vivid, position-independent electromagnetic background palette.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_electromagnetic_background_r170
      tests/test_predator_electromagnetic_background_r170.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_electromagnetic_background_r170 PRIVATE include)
  target_compile_options(test_predator_electromagnetic_background_r170 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_electromagnetic_background_r170 PRIVATE m)
  add_test(NAME predator_electromagnetic_background_r170
      COMMAND test_predator_electromagnetic_background_r170)
endif()

# RE39 R173: Infrared scenery must show red/pink at common brightness values;
# leave actor palettes, Electromagnetic and Thermal completely unchanged.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_infrared_background_r173
      tests/test_predator_infrared_background_r173.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_infrared_background_r173 PRIVATE include)
  target_compile_options(test_predator_infrared_background_r173 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_infrared_background_r173 PRIVATE m)
  add_test(NAME predator_infrared_background_r173
      COMMAND test_predator_infrared_background_r173)
endif()

# RE39 R175: background distance must come from the actual wall/flat renderer,
# not source texture brightness. Keep the IR world and actors independent.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_predator_infrared_background_depth_r175
      tests/test_predator_infrared_background_depth_r175.c
      new/jaguar/avp_gpu/thermal_vision.c)
  target_include_directories(test_predator_infrared_background_depth_r175 PRIVATE include)
  target_compile_options(test_predator_infrared_background_depth_r175 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_predator_infrared_background_depth_r175 PRIVATE m)
  add_test(NAME predator_infrared_background_depth_r175
      COMMAND test_predator_infrared_background_depth_r175)
endif()

# RE39 R192: exact signed 68000 word decode/packing in AMP search state,
# projectile velocities and level-save positions. Production AMP is included
# by the fixture, while unrelated runtime functions are discarded at link.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_register_words_r192
      tests/test_amp_register_words_r192.c
      src/game/runtime.c
      tests/amp_link_stubs.c)
  target_include_directories(test_amp_register_words_r192 PRIVATE include)
  target_compile_options(test_amp_register_words_r192 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_amp_register_words_r192 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_register_words_r192 PRIVATE m)
  add_test(NAME amp_register_words_r192 COMMAND test_amp_register_words_r192)
endif()

# RE39 R193: host-only independently testable preferences and Predator jammed door.
add_executable(test_crosshair_preferences_r193
  tests/test_crosshair_preferences_r193.c new/multiplayer/local_mp_input.c)
target_include_directories(test_crosshair_preferences_r193 PRIVATE include new/multiplayer)
target_link_libraries(test_crosshair_preferences_r193 PRIVATE m)
add_executable(test_predator_jammed_door_r193
  tests/test_predator_jammed_door_r193.c src/game/doors.c)
target_include_directories(test_predator_jammed_door_r193 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_crosshair_preferences_r193 PRIVATE -Wall -Wextra -Werror)
  target_compile_options(test_predator_jammed_door_r193 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME crosshair_preferences_r193 COMMAND test_crosshair_preferences_r193)
add_test(NAME predator_jammed_door_r193 COMMAND test_predator_jammed_door_r193)

# RE41 R235: ROM-derived InitAccess and original numeric door security policy.
add_executable(test_alien_security_door_r235
  tests/test_alien_security_door_r235.c src/game/doors.c)
target_include_directories(test_alien_security_door_r235 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_alien_security_door_r235 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME alien_security_door_r235 COMMAND test_alien_security_door_r235)

# RE39 R194: prevent the single-player presentation path dropping the P1 crosshair.
add_executable(test_crosshair_singleplayer_entry_r194
  tests/test_crosshair_singleplayer_entry_r194.c)
target_include_directories(test_crosshair_singleplayer_entry_r194 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_crosshair_singleplayer_entry_r194 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME crosshair_singleplayer_entry_r194
  COMMAND test_crosshair_singleplayer_entry_r194
    ${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/pc_video_x11.c)

# RE39 R195: crosshair sway is interpolated by the same host presentation
# calculation as the first-person weapon; ordinary Jaguar aim stays unchanged.
add_executable(test_crosshair_smooth_presentation_r195
  tests/test_crosshair_smooth_presentation_r195.c
  new/jaguar/avp_gpu/presentation_interp.c)
target_include_directories(test_crosshair_smooth_presentation_r195 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_crosshair_smooth_presentation_r195 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
target_link_libraries(test_crosshair_smooth_presentation_r195 PRIVATE m)
add_test(NAME crosshair_smooth_presentation_r195
  COMMAND test_crosshair_smooth_presentation_r195
    ${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/pc_video_x11.c
    ${CMAKE_CURRENT_SOURCE_DIR}/new/jaguar/avp_gpu/software_renderer.c)

# RE39 R196: source-linked optional melee/all-attacks sway bypass.
add_executable(test_player_center_aim_r196 tests/test_player_center_aim_r196.c
  tests/player_center_aim_link_stubs_r252.c src/game/player.c)
target_include_directories(test_player_center_aim_r196 PRIVATE include)
target_compile_options(test_player_center_aim_r196 PRIVATE
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_player_center_aim_r196 PRIVATE -Wl,--gc-sections)
target_link_libraries(test_player_center_aim_r196 PRIVATE m)
add_test(NAME player_center_aim_r196 COMMAND test_player_center_aim_r196)

# RE39 R196 follow-up: hidden weapon sprite must not disable crosshair
# interpolation; both SP and local-MP compositor paths use the same helper.
add_executable(test_crosshair_hidden_smooth_r196
  tests/test_crosshair_hidden_smooth_r196.c
  new/jaguar/avp_gpu/presentation_interp.c)
target_include_directories(test_crosshair_hidden_smooth_r196 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_crosshair_hidden_smooth_r196 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
target_link_libraries(test_crosshair_hidden_smooth_r196 PRIVATE m)
add_test(NAME crosshair_hidden_smooth_r196
  COMMAND test_crosshair_hidden_smooth_r196
    ${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/pc_video_x11.c)

# RE40 R197: first-person OP art can extend past the 4:3 HUD into extra
# widescreen columns.  The original 4:3 weapon/HUD raster remains untouched.
add_executable(test_fp_wide_weapon_r197 tests/test_fp_wide_weapon_r197.c
  new/jaguar/avp_gpu/fp_overlay.c)
target_include_directories(test_fp_wide_weapon_r197 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_fp_wide_weapon_r197 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME fp_wide_weapon_r197 COMMAND test_fp_wide_weapon_r197)

# RE40 R198: reference-machine hardware reimplementation of TOM OP FIRSTPIX.
# Tests authentic 68000->OLP->OP execution with synthetic cartridge content.
if(AVP_BUILD_JAG_REFERENCE)
  add_executable(test_jag_machine_firstpix_r198 tests/test_jag_machine_firstpix_r198.c)
  target_link_libraries(test_jag_machine_firstpix_r198 PRIVATE avp_jag_refmachine)
  target_include_directories(test_jag_machine_firstpix_r198 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_firstpix_r198 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_firstpix_r198 COMMAND test_jag_machine_firstpix_r198)
endif()

# RE40 R199: original TOM RGB24 SCBITOBJ supported 1x horizontal path.
# Synthetic 68000 cartridge checks actual OP frame pixels and write-back.
if(AVP_BUILD_JAG_REFERENCE)
  add_executable(test_jag_machine_rgb24_scaled_r199
    tests/test_jag_machine_rgb24_scaled_r199.c)
  target_link_libraries(test_jag_machine_rgb24_scaled_r199 PRIVATE avp_jag_refmachine)
  target_include_directories(test_jag_machine_rgb24_scaled_r199 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_rgb24_scaled_r199 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_rgb24_scaled_r199
    COMMAND test_jag_machine_rgb24_scaled_r199)
endif()

# RE40 R200: RGB24 scaled TOM FIRSTPIX selects the second source pixel.
# Full synthetic cartridge covers 1x presentation, reflection and clipping.
if(AVP_BUILD_JAG_REFERENCE)
  add_executable(test_jag_machine_rgb24_firstpix_r200
    tests/test_jag_machine_rgb24_firstpix_r200.c)
  target_link_libraries(test_jag_machine_rgb24_firstpix_r200 PRIVATE avp_jag_refmachine)
  target_include_directories(test_jag_machine_rgb24_firstpix_r200 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_rgb24_firstpix_r200 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_rgb24_firstpix_r200
    COMMAND test_jag_machine_rgb24_firstpix_r200)
endif()

# RE40 R201: the manual's A->C->A cocoon, real 10-Hz first-person
# animation and charge depletion, plus the unmodified original stun/kill
# branching. This is a regression, NOT a host health/damage override.
add_executable(test_alien_cocoon_cadence_r201
  tests/test_alien_cocoon_cadence_r201.c
  src/game/amp.c src/game/mazescrn.c src/game/player.c
  src/game/hud.c src/game/savegame.c src/game/eeprom.c src/game/runtime.c)
target_include_directories(test_alien_cocoon_cadence_r201 PRIVATE include)
target_compile_options(test_alien_cocoon_cadence_r201 PRIVATE
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_alien_cocoon_cadence_r201 PRIVATE -Wl,--gc-sections)
target_link_libraries(test_alien_cocoon_cadence_r201 PRIVATE m)
add_test(NAME alien_cocoon_cadence_r201 COMMAND test_alien_cocoon_cadence_r201)

# RE40 R201: enemy Predator Smart Disc must not play DISK1 twice during
# attack-selection and projectile spawn.  The projectile still spawns frame 3.
add_executable(test_amp_predator_disc_single_cue_r201
  tests/test_amp_predator_disc_single_cue_r201.c
  src/game/amp.c src/game/runtime.c tests/amp_link_stubs.c)
target_include_directories(test_amp_predator_disc_single_cue_r201 PRIVATE include)
target_compile_options(test_amp_predator_disc_single_cue_r201 PRIVATE
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_amp_predator_disc_single_cue_r201 PRIVATE -Wl,--gc-sections)
target_link_libraries(test_amp_predator_disc_single_cue_r201 PRIVATE m)
add_test(NAME amp_predator_disc_single_cue_r201
  COMMAND test_amp_predator_disc_single_cue_r201)

# RE40 R202: source-linked Predator player Smart Disc and Shoulder Cannon
# playback/launch isolation from the R201 enemy Smart Disc cue correction.
# Uses the real 10 Hz PlayerWeapons -> update_wep -> player_weapon chain.
add_executable(test_predator_player_projectile_r202
  tests/test_predator_player_projectile_r202.c
  src/game/amp.c src/game/mazescrn.c src/game/player.c
  src/game/hud.c src/game/savegame.c src/game/eeprom.c src/game/runtime.c)
target_include_directories(test_predator_player_projectile_r202 PRIVATE include)
target_compile_options(test_predator_player_projectile_r202 PRIVATE
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_predator_player_projectile_r202 PRIVATE -Wl,--gc-sections)
target_link_libraries(test_predator_player_projectile_r202 PRIVATE m)
add_test(NAME predator_player_projectile_r202 COMMAND test_predator_player_projectile_r202)

# RE40 R203: PC-only post-processing Vignette, including viewport isolation.
add_executable(test_vignette_r203 tests/test_vignette_r203.c new/pc_host/src/pc_vignette.c)
target_include_directories(test_vignette_r203 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_vignette_r203 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME vignette_r203 COMMAND test_vignette_r203)

# RE40 R204: host Deathmatch scoreboard covers 4 humans + 32 bots at any
# supported window size, with pagination only when necessary.
if(AVP_BUILD_LOCAL_MULTIPLAYER)
  add_test(NAME local_mp_scoreboard_layout_r204
    COMMAND test_local_mp_scoreboard_layout_r204)
endif()

# RE40 R205: Jaguar TOM STOP publishes its payload in OB0-3 before
# signaling the host Object interrupt (Technical Reference Rev.8 p.20).
if(AVP_BUILD_JAG_REFERENCE)
  add_executable(test_jag_machine_op_stop_payload_r205
    tests/test_jag_machine_op_stop_payload_r205.c)
  target_link_libraries(test_jag_machine_op_stop_payload_r205 PRIVATE avp_jag_refmachine)
  target_include_directories(test_jag_machine_op_stop_payload_r205 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_op_stop_payload_r205 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_op_stop_payload_r205
    COMMAND test_jag_machine_op_stop_payload_r205)
endif()

# RE40 R206: both physical Jaguar controller ports, reversed port-2 row
# selectors, and independent JERRY JOYSTICK/JOYBUTS inputs.
if(TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_jerry_two_ports_r206
    tests/test_jag_machine_jerry_two_ports_r206.c)
  target_link_libraries(test_jag_machine_jerry_two_ports_r206 PRIVATE avp_jag_refmachine)
  target_include_directories(test_jag_machine_jerry_two_ports_r206 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_jerry_two_ports_r206 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_jerry_two_ports_r206
    COMMAND test_jag_machine_jerry_two_ports_r206)
endif()

# RE40 R207: cartridge-level TOM OP branch CC=4/HC10 and STOP INT flag.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_op_branch_stop_r207
    tests/test_jag_machine_op_branch_stop_r207.c)
  target_include_directories(test_jag_machine_op_branch_stop_r207 PRIVATE include)
  target_link_libraries(test_jag_machine_op_branch_stop_r207 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_op_branch_stop_r207 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_op_branch_stop_r207
    COMMAND test_jag_machine_op_branch_stop_r207)
endif()

# RE40 R208: original Jaguar console VDE>VP Object Processor start behavior.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_vde_console_wrap_r208
    tests/test_jag_machine_vde_console_wrap_r208.c)
  target_include_directories(test_jag_machine_vde_console_wrap_r208 PRIVATE include)
  target_link_libraries(test_jag_machine_vde_console_wrap_r208 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_vde_console_wrap_r208 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_vde_console_wrap_r208
    COMMAND test_jag_machine_vde_console_wrap_r208)
endif()

# RE40 R209: second horizontal-half Object Processor start and split line.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_hdb2_split_r209
    tests/test_jag_machine_hdb2_split_r209.c)
  target_include_directories(test_jag_machine_hdb2_split_r209 PRIVATE include)
  target_link_libraries(test_jag_machine_hdb2_split_r209 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_hdb2_split_r209 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_hdb2_split_r209
    COMMAND test_jag_machine_hdb2_split_r209)
endif()

# RE40 R210: TOM VMODE BGEN is active in CRY and RGB16, not direct/RGB24.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_vmode_bgen_r210
    tests/test_jag_machine_vmode_bgen_r210.c)
  target_include_directories(test_jag_machine_vmode_bgen_r210 PRIVATE include)
  target_link_libraries(test_jag_machine_vmode_bgen_r210 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_vmode_bgen_r210 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_vmode_bgen_r210
    COMMAND test_jag_machine_vmode_bgen_r210)
endif()

# RE40 R211: genuine interlaced BITOBJ source-row/HEIGHT stepping,
# checked with a self-contained 68000 cartridge, not a framebuffer mock.
if(TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_interlace_bitmap_r211
    tests/test_jag_machine_interlace_bitmap_r211.c)
  target_link_libraries(test_jag_machine_interlace_bitmap_r211 PRIVATE avp_jag_refmachine)
  target_include_directories(test_jag_machine_interlace_bitmap_r211 PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_interlace_bitmap_r211 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_interlace_bitmap_r211
    COMMAND test_jag_machine_interlace_bitmap_r211)
endif()

# RE40 R212: three-bit TOM branch condition parity for the standalone
# Object Processor trace (including the HDB2 HC10 horizontal branch).
add_executable(test_op_branch_hc10_r212 tests/test_op_branch_hc10_r212.c)
target_link_libraries(test_op_branch_hc10_r212 PRIVATE avp_jaguar_compat)
target_include_directories(test_op_branch_hc10_r212 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_op_branch_hc10_r212 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME op_branch_hc10_r212 COMMAND test_op_branch_hc10_r212)

# RE40 R213: standalone TOM BITOBJ writeback at interlaced VP field parity.
# The old non-interlaced API and active host renderer remain unchanged.
add_executable(test_op_interlaced_writeback_r213
  tests/test_op_interlaced_writeback_r213.c)
target_link_libraries(test_op_interlaced_writeback_r213 PRIVATE avp_jaguar_compat)
target_include_directories(test_op_interlaced_writeback_r213 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_op_interlaced_writeback_r213 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME op_interlaced_writeback_r213
  COMMAND test_op_interlaced_writeback_r213)

# RE40 R214: TOM host reference frame-capture widths are per-field,
# including register changes, zero-output fields and HDB2 split segments.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_field_width_r214
    tests/test_jag_machine_field_width_r214.c)
  target_include_directories(test_jag_machine_field_width_r214 PRIVATE include)
  target_link_libraries(test_jag_machine_field_width_r214 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_field_width_r214 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_field_width_r214
    COMMAND test_jag_machine_field_width_r214)
endif()

# RE40 R216: TOM HDB1/HDB2 events must occur AFTER earlier 68000 bus
# writes at their programmed horizontal positions. Baseline R215 fails.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_hdb_timing_r216
    tests/test_jag_machine_hdb_timing_r216.c)
  target_include_directories(test_jag_machine_hdb_timing_r216 PRIVATE include)
  target_link_libraries(test_jag_machine_hdb_timing_r216 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_hdb_timing_r216 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_hdb_timing_r216
    COMMAND test_jag_machine_hdb_timing_r216)
endif()

# RE40 R215: optional F12 Miscellaneous assist for the full-charge Alien
# Claw -> Tail -> Claw cocoon combo, off for Jaguar gameplay fidelity.
add_executable(test_alien_infinite_ammo_assist_r215
  tests/test_alien_infinite_ammo_assist_r215.c
  src/game/amp.c src/game/mazescrn.c src/game/player.c
  src/game/hud.c src/game/savegame.c src/game/eeprom.c src/game/runtime.c)
target_include_directories(test_alien_infinite_ammo_assist_r215 PRIVATE include)
target_compile_options(test_alien_infinite_ammo_assist_r215 PRIVATE
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_alien_infinite_ammo_assist_r215 PRIVATE -Wl,--gc-sections)
target_link_libraries(test_alien_infinite_ammo_assist_r215 PRIVATE m)
add_test(NAME alien_infinite_ammo_assist_r215 COMMAND test_alien_infinite_ammo_assist_r215)

# R215: direct F12 Misc entry, default-off and settings.cfg roundtrip.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_alien_cocoon_assist_menu_r215
      new/pc_host/tests/test_alien_cocoon_assist_menu_r215.c)
  target_include_directories(test_alien_cocoon_assist_menu_r215 PRIVATE
      include new/multiplayer new/map_maker new/game_micro_optimization)
  target_compile_definitions(test_alien_cocoon_assist_menu_r215 PRIVATE
      AVP_LOCAL_MULTIPLAYER=1 AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE=1)
  target_compile_options(test_alien_cocoon_assist_menu_r215 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_alien_cocoon_assist_menu_r215 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_alien_cocoon_assist_menu_r215 PRIVATE X11 m ${CMAKE_DL_LIBS})
  # RE47 R342 audit: these source-inclusion menu regressions pull the
  # current high-resolution loader, filter cache and Vulkan selection entry
  # points from pc_video_x11.c. Link their real implementations, not test
  # stubs, so the historical menu assertions keep testing real behavior.
  target_sources(test_alien_cocoon_assist_menu_r215 PRIVATE
    new/pc_host/src/pc_hi_res_textures.c
    new/pc_host/src/pc_hi_res_sprites.c
    new/pc_host/src/pc_vulkan_probe.c
    new/jaguar/avp_gpu/texture_filter.c
    new/jaguar/avp_gpu/visual_bridge.cpp
    new/jaguar/hw/cry.c)
  set_target_properties(test_alien_cocoon_assist_menu_r215 PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(test_alien_cocoon_assist_menu_r215 PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})
  add_test(NAME alien_cocoon_assist_menu_r215 COMMAND test_alien_cocoon_assist_menu_r215)
endif()

# RE40 R217: HDB1-only display start in HC10=1 must not run at HC10=0.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_hdb1_second_half_r217
    tests/test_jag_machine_hdb1_second_half_r217.c)
  target_include_directories(test_jag_machine_hdb1_second_half_r217 PRIVATE include)
  target_link_libraries(test_jag_machine_hdb1_second_half_r217 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_hdb1_second_half_r217 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_hdb1_second_half_r217
    COMMAND test_jag_machine_hdb1_second_half_r217)
endif()

# RE40 R218: ordered independent HDB1/HDB2 starts within the same HC10 half.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_hdb_same_half_r218
    tests/test_jag_machine_hdb_same_half_r218.c)
  target_include_directories(test_jag_machine_hdb_same_half_r218 PRIVATE include)
  target_link_libraries(test_jag_machine_hdb_same_half_r218 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_hdb_same_half_r218 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_hdb_same_half_r218
    COMMAND test_jag_machine_hdb_same_half_r218)
endif()

# RE40 R219: HDB1/HDB2 compare independently, including reverse-ordered
# starts and a lone HDB2 event.  Run actual synthetic 68000 cartridges.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_hdb_reverse_r219
    tests/test_jag_machine_hdb_reverse_r219.c)
  target_include_directories(test_jag_machine_hdb_reverse_r219 PRIVATE include)
  target_link_libraries(test_jag_machine_hdb_reverse_r219 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_hdb_reverse_r219 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_hdb_reverse_r219
    COMMAND test_jag_machine_hdb_reverse_r219)
endif()

# RE41 R220: SCBITOBJ VSCALE may consume multiple source rows even after
# HEIGHT saturates at zero; DATA/REMAINDER still follow the vertical step.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_scaled_height_saturation_r220
    tests/test_jag_machine_scaled_height_saturation_r220.c)
  target_include_directories(test_jag_machine_scaled_height_saturation_r220 PRIVATE include)
  target_link_libraries(test_jag_machine_scaled_height_saturation_r220 PRIVATE avp_jag_refmachine avp_jaguar_compat)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_scaled_height_saturation_r220 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_scaled_height_saturation_r220
    COMMAND test_jag_machine_scaled_height_saturation_r220)
endif()

# RE41 R221: actual TOM HC/VC readback, through synthetic 68000 reads.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_beam_counters_r221
    tests/test_jag_machine_beam_counters_r221.c)
  target_include_directories(test_jag_machine_beam_counters_r221 PRIVATE include)
  target_link_libraries(test_jag_machine_beam_counters_r221 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_beam_counters_r221 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_beam_counters_r221
    COMMAND test_jag_machine_beam_counters_r221)
endif()

# RE41 R222: GPUOBJ bits 3..63 are opaque ISR payload; BRANCH gates by VC.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_gpuobj_payload_r222
    tests/test_jag_machine_gpuobj_payload_r222.c)
  target_include_directories(test_jag_machine_gpuobj_payload_r222 PRIVATE include)
  target_link_libraries(test_jag_machine_gpuobj_payload_r222 PRIVATE
    avp_jag_refmachine avp_jaguar_compat)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_gpuobj_payload_r222 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_gpuobj_payload_r222
    COMMAND test_jag_machine_gpuobj_payload_r222)
endif()

# RE41 R223: optional 1994-pattern automap void/corner completion. Native-only;
# must not mutate the original MAZE5 persistent map or reveal walkable cells.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_automap_hatch_completion_r223
    tests/test_automap_hatch_completion_r223.c
    new/jaguar/avp_gpu/automap.c)
  target_include_directories(test_automap_hatch_completion_r223 PRIVATE include)
  target_compile_options(test_automap_hatch_completion_r223 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_automap_hatch_completion_r223 PRIVATE
    -Wl,--gc-sections)
  add_test(NAME automap_hatch_completion_r223
    COMMAND test_automap_hatch_completion_r223)
endif()

# RE41 R234: repair textured-tile enclosed gaps, interrupted rims and
# short hatch stripes without painting explored walkable map cells.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_automap_hatch_completion_r234
    tests/test_automap_hatch_completion_r234.c
    new/jaguar/avp_gpu/automap.c)
  target_include_directories(test_automap_hatch_completion_r234 PRIVATE include)
  target_compile_options(test_automap_hatch_completion_r234 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_automap_hatch_completion_r234 PRIVATE
    -Wl,--gc-sections)
  add_test(NAME automap_hatch_completion_r234
    COMMAND test_automap_hatch_completion_r234)
endif()

# RE41 R224: VI==0 is an in-range TOM half-line comparator at field wrap.
# Never match the nonexistent VP+1 half-line before resetting VC to zero.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_vi_zero_wrap_r224
    tests/test_jag_machine_vi_zero_wrap_r224.c)
  target_include_directories(test_jag_machine_vi_zero_wrap_r224 PRIVATE include)
  target_link_libraries(test_jag_machine_vi_zero_wrap_r224 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_vi_zero_wrap_r224 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_vi_zero_wrap_r224
    COMMAND test_jag_machine_vi_zero_wrap_r224)
endif()

# RE41 R225: OP GPU interrupt object must resume on any OBF write, including
# writes with bit 15 set. No register bits may serve as private busy state.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_obf_any_write_r225
    tests/test_jag_machine_obf_any_write_r225.c)
  target_include_directories(test_jag_machine_obf_any_write_r225 PRIVATE include)
  target_link_libraries(test_jag_machine_obf_any_write_r225 PRIVATE avp_jag_refmachine)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_obf_any_write_r225 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_obf_any_write_r225
    COMMAND test_jag_machine_obf_any_write_r225)
endif()

# RE41 R226: TOM Jerry input is edge-triggered; TOM ACK does not clear JINT.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_jerry_edge_irq_r226
    tests/test_jag_machine_jerry_edge_irq_r226.c)
  target_include_directories(test_jag_machine_jerry_edge_irq_r226 PRIVATE include)
  target_link_libraries(test_jag_machine_jerry_edge_irq_r226 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_jerry_edge_irq_r226 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_jerry_edge_irq_r226
    COMMAND test_jag_machine_jerry_edge_irq_r226)
endif()

# RE41 R227: host-only HUD cheat, real 16.16 actor positions; upright labels
# even as the map rotates, in standard and smooth presentation.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_automap_enemy_letters_r227
    tests/test_automap_enemy_letters_r227.c
    new/jaguar/avp_gpu/presentation_interp.c)
  target_include_directories(test_automap_enemy_letters_r227 PRIVATE include)
  target_compile_options(test_automap_enemy_letters_r227 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_automap_enemy_letters_r227 PRIVATE m)
  add_test(NAME automap_enemy_letters_r227
    COMMAND test_automap_enemy_letters_r227)
endif()

# RE41 R228: Rev.8 TOM line-buffer 32-bit write-only aliases (+8000h).
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_lbuf_write_alias_r228
    tests/test_jag_machine_lbuf_write_alias_r228.c)
  target_include_directories(test_jag_machine_lbuf_write_alias_r228 PRIVATE include)
  target_link_libraries(test_jag_machine_lbuf_write_alias_r228 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_lbuf_write_alias_r228 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_lbuf_write_alias_r228
    COMMAND test_jag_machine_lbuf_write_alias_r228)
endif()

# RE41 R229: map markers must omit lethal/dead static Facehugger eggs.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_automap_destroyed_eggs_r229
    tests/test_automap_destroyed_eggs_r229.c
    new/jaguar/avp_gpu/presentation_interp.c)
  target_include_directories(test_automap_destroyed_eggs_r229 PRIVATE include)
  target_compile_options(test_automap_destroyed_eggs_r229 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_automap_destroyed_eggs_r229 PRIVATE m)
  add_test(NAME automap_destroyed_eggs_r229
    COMMAND test_automap_destroyed_eggs_r229)
endif()

# RE41 R229: TOM OB0..3 latch words are low-phrase-word first.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_ob_word_order_r229
    tests/test_jag_machine_ob_word_order_r229.c)
  target_include_directories(test_jag_machine_ob_word_order_r229 PRIVATE include)
  target_link_libraries(test_jag_machine_ob_word_order_r229 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_ob_word_order_r229 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_ob_word_order_r229
    COMMAND test_jag_machine_ob_word_order_r229)
endif()

# RE41 R230: an unreleased GPU object must not execute following OP objects.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_gpuobj_unreleased_r230
    tests/test_jag_machine_gpuobj_unreleased_r230.c)
  target_include_directories(test_jag_machine_gpuobj_unreleased_r230 PRIVATE include)
  target_link_libraries(test_jag_machine_gpuobj_unreleased_r230 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_gpuobj_unreleased_r230 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_gpuobj_unreleased_r230
    COMMAND test_jag_machine_gpuobj_unreleased_r230)
endif()

# RE41 R232: guest-programmed TOM VP=0 must represent one half-line, not NTSC.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_vp_zero_r232
    tests/test_jag_machine_vp_zero_r232.c)
  target_include_directories(test_jag_machine_vp_zero_r232 PRIVATE include)
  target_link_libraries(test_jag_machine_vp_zero_r232 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_vp_zero_r232 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_vp_zero_r232
    COMMAND test_jag_machine_vp_zero_r232)
endif()

# RE41 R233: MTOI unpacks the IEEE implicit mantissa bit and negates for sign.
add_executable(test_jrisc_mtoi_sign_mantissa_r233
  tests/test_jrisc_mtoi_sign_mantissa_r233.c)
target_link_libraries(test_jrisc_mtoi_sign_mantissa_r233 PRIVATE avp_jaguar_trm)
target_include_directories(test_jrisc_mtoi_sign_mantissa_r233 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_jrisc_mtoi_sign_mantissa_r233 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME jrisc_mtoi_sign_mantissa_r233
  COMMAND test_jrisc_mtoi_sign_mantissa_r233)

# RE41 R236: GPU/DSP BIG_INSTR reverses 16-bit fetch lanes, MOVEI remains low/high.
add_executable(test_jrisc_big_instr_order_r236
  tests/test_jrisc_big_instr_order_r236.c)
target_link_libraries(test_jrisc_big_instr_order_r236 PRIVATE avp_jaguar_trm)
target_include_directories(test_jrisc_big_instr_order_r236 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_jrisc_big_instr_order_r236 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME jrisc_big_instr_order_r236
  COMMAND test_jrisc_big_instr_order_r236)

# RE41 R237: security authorization follows original playable-species rules.
add_executable(test_species_security_door_r237
  tests/test_species_security_door_r237.c src/game/doors.c)
target_include_directories(test_species_security_door_r237 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_species_security_door_r237 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME species_security_door_r237 COMMAND test_species_security_door_r237)

# RE41 R238: GPU/DSP BIG_IO 68000-facing word order and direct writes.
add_executable(test_jag_big_io_word_order_r238
  tests/test_jag_big_io_word_order_r238.c)
target_include_directories(test_jag_big_io_word_order_r238 PRIVATE include)
target_link_libraries(test_jag_big_io_word_order_r238 PRIVATE avp_jag_refmachine)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_jag_big_io_word_order_r238 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME jag_big_io_word_order_r238
  COMMAND test_jag_big_io_word_order_r238)

# RE41 R239: GPU/DSP SINGLE_STEP/GO/STOP documented instruction pacing.
add_executable(test_jrisc_single_step_r239
  tests/test_jrisc_single_step_r239.c)
target_link_libraries(test_jrisc_single_step_r239 PRIVATE avp_jaguar_trm)
target_include_directories(test_jrisc_single_step_r239 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_jrisc_single_step_r239 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME jrisc_single_step_r239
  COMMAND test_jrisc_single_step_r239)

# RE41 R240: raw retail Jaguar Alien controls and PC adapter parity.
add_executable(test_alien_retail_strafe_r240
  tests/test_alien_retail_strafe_r240.c
  src/game/player.c)
target_include_directories(test_alien_retail_strafe_r240 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_alien_retail_strafe_r240 PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(test_alien_retail_strafe_r240 PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(test_alien_retail_strafe_r240 PRIVATE m)
add_test(NAME alien_retail_strafe_r240 COMMAND test_alien_retail_strafe_r240)

# RE41 R241: independent keypad 7/9 side-step, ROM-sourced positions.
add_executable(test_alien_keypad_strafe_r241
  tests/test_alien_keypad_strafe_r241.c
  src/game/player.c)
target_include_directories(test_alien_keypad_strafe_r241 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_alien_keypad_strafe_r241 PRIVATE -ffunction-sections -fdata-sections -UNDEBUG)
  target_link_options(test_alien_keypad_strafe_r241 PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(test_alien_keypad_strafe_r241 PRIVATE m)
add_test(NAME alien_keypad_strafe_r241 COMMAND test_alien_keypad_strafe_r241)

# RE42 R243: preserve the original VDE=$FFFF every-line OP exception.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_vde_ffff_r243
    tests/test_jag_machine_vde_ffff_r243.c)
  target_include_directories(test_jag_machine_vde_ffff_r243 PRIVATE include)
  target_link_libraries(test_jag_machine_vde_ffff_r243 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_vde_ffff_r243 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_vde_ffff_r243
    COMMAND test_jag_machine_vde_ffff_r243)
endif()

# RE42 R244: scaled OP headers occupy three phrases at 32-byte boundaries.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_op_scaled_alignment_r244
    tests/test_jag_op_scaled_alignment_r244.c)
  target_include_directories(test_jag_op_scaled_alignment_r244 PRIVATE include)
  target_link_libraries(test_jag_op_scaled_alignment_r244 PRIVATE
    avp_jag_refmachine avp_jaguar_compat)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_op_scaled_alignment_r244 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_op_scaled_alignment_r244
    COMMAND test_jag_op_scaled_alignment_r244)
endif()

# RE42 R245: TOM VI asserts once per complete interlaced frame, not per field.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_interlaced_vi_r245
    tests/test_jag_machine_interlaced_vi_r245.c)
  target_include_directories(test_jag_machine_interlaced_vi_r245 PRIVATE include)
  target_link_libraries(test_jag_machine_interlaced_vi_r245 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_interlaced_vi_r245 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_interlaced_vi_r245
    COMMAND test_jag_machine_interlaced_vi_r245)
endif()

# RE42 R246: JOYSTICK bit 8 is the external I2S audio mute gate.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_joy_audio_mute_r246
    tests/test_jag_machine_joy_audio_mute_r246.c)
  target_include_directories(test_jag_machine_joy_audio_mute_r246 PRIVATE include)
  target_link_libraries(test_jag_machine_joy_audio_mute_r246 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_joy_audio_mute_r246 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_joy_audio_mute_r246
    COMMAND test_jag_machine_joy_audio_mute_r246)
endif()

# RE42 R247 full sweep: ensure the R240 retail C+Right sidestep is not
# mistaken for a digital turn after a completed interpolated Quick Turn.
add_executable(test_pc_quick_turn_player_r247
  tests/test_pc_quick_turn_player_r266.c
  src/game/player.c)
target_include_directories(test_pc_quick_turn_player_r247 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_quick_turn_player_r247 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_quick_turn_player_r247 PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(test_pc_quick_turn_player_r247 PRIVATE m)
add_test(NAME pc_quick_turn_player_r247 COMMAND test_pc_quick_turn_player_r247)

# R247 full sweep: previously unregistered R260 idle-mouse regression now
# checks both original Marine C+Right sidestep and the raw digital turn.
add_executable(test_pc_mouse_idle_precision_r247
  tests/test_pc_mouse_idle_precision_r260.c
  src/game/player.c)
target_include_directories(test_pc_mouse_idle_precision_r247 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_mouse_idle_precision_r247 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_mouse_idle_precision_r247 PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(test_pc_mouse_idle_precision_r247 PRIVATE m)
add_test(NAME pc_mouse_idle_precision_r247 COMMAND test_pc_mouse_idle_precision_r247)


# Validation-only Python regression, optional on platforms lacking Python/grep.
if(UNIX)
  find_program(AVP_R247_PYTHON3 python3)
  if(AVP_R247_PYTHON3)
    add_test(NAME validate_marker_filter_r247
      COMMAND "${AVP_R247_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_validate_marker_filter_r247.py")
  endif()
endif()

# R247 full sweep: register the historical SDK errata fixture so future
# BIG_INSTR lane-order changes cannot silently invalidate its opcodes.
add_executable(test_atari_errata_s18_r247
  tests/test_atari_errata_s18.c
  new/jaguar/hw/jrisc.c
  new/jaguar/hw/blitter_trm.c
  new/jaguar/hw/object_processor.c
  new/jaguar/hw/cry.c)
target_include_directories(test_atari_errata_s18_r247 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_atari_errata_s18_r247 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
target_link_libraries(test_atari_errata_s18_r247 PRIVATE m)
add_test(NAME atari_errata_s18_r247 COMMAND test_atari_errata_s18_r247)

# R247 full sweep: restore and register original GPU bus/provenance witnesses.
# Their synthetic opcodes intentionally use the documented BIG_INSTR=1 lane.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_gpu_view_bus_s22_r247
    tests/test_jag_gpu_view_bus_s22.c)
  add_executable(test_gpu_cpu_provenance_s23_r247
    tests/test_gpu_cpu_provenance_s23.c)
  foreach(tgt IN ITEMS test_jag_gpu_view_bus_s22_r247 test_gpu_cpu_provenance_s23_r247)
    target_include_directories(${tgt} PRIVATE include)
    target_link_libraries(${tgt} PRIVATE avp_jag_refmachine)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(${tgt} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
  endforeach()
  add_test(NAME jag_gpu_view_bus_s22_r247 COMMAND test_jag_gpu_view_bus_s22_r247)
  add_test(NAME gpu_cpu_provenance_s23_r247 COMMAND test_gpu_cpu_provenance_s23_r247)
endif()

# RE42 R248: SCBITOBJ final source row with HEIGHT=0 and REMAINDER>0.
if(AVP_BUILD_JAG_REFERENCE)
  add_executable(test_jag_machine_scaled_zero_height_r248
    tests/test_jag_machine_scaled_zero_height_r248.c)
  target_include_directories(test_jag_machine_scaled_zero_height_r248 PRIVATE include)
  target_link_libraries(test_jag_machine_scaled_zero_height_r248 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_scaled_zero_height_r248 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_scaled_zero_height_r248
    COMMAND test_jag_machine_scaled_zero_height_r248)
endif()

# RE42 R249: deferred TOM GPUOBJ list continuation after a late OBF write.
if(AVP_BUILD_JAG_REFERENCE)
  add_executable(test_jag_machine_gpuobj_deferred_r249
    tests/test_jag_machine_gpuobj_deferred_r249.c)
  target_include_directories(test_jag_machine_gpuobj_deferred_r249 PRIVATE include)
  target_link_libraries(test_jag_machine_gpuobj_deferred_r249 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_gpuobj_deferred_r249 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_gpuobj_deferred_r249
    COMMAND test_jag_machine_gpuobj_deferred_r249)
endif()

# RE42 R250: unscaled BITOBJ requires 16-byte header alignment (TRM Rev.8 p.18).
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_op_bitmap_alignment_r250
    tests/test_jag_op_bitmap_alignment_r250.c)
  target_include_directories(test_jag_op_bitmap_alignment_r250 PRIVATE include)
  target_link_libraries(test_jag_op_bitmap_alignment_r250 PRIVATE
    avp_jag_refmachine avp_jaguar_compat)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_op_bitmap_alignment_r250 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_op_bitmap_alignment_r250
    COMMAND test_jag_op_bitmap_alignment_r250)
endif()

# RE42 R253: exercise the real original AMP allocator's multi-world snapshots
# and the optional bitmap/destination capture cross-check in registered CTest.
# These isolated tests link fail-on-use test-only callbacks; production does not.
if(TARGET test_local_mp_amp_native AND TARGET test_game_micro_amp_scope)
  add_test(NAME local_mp_amp_native_r253 COMMAND test_local_mp_amp_native)
  add_test(NAME game_micro_amp_scope_r253 COMMAND test_game_micro_amp_scope)
endif()

# RE42 R254: bring the existing original Queen campaign regression into CTest.
# Tests defined in the multiplayer subdirectory precede enable_testing()
# in this historical tree, so register at the top level after it is enabled.
if(TARGET test_local_mp_queen_campaign)
  add_test(NAME local_mp_queen_campaign_r254 COMMAND test_local_mp_queen_campaign)
endif()

# RE42 R257: JERRY JPIT live prescaler/divider counter readback.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_jerry_timer_readback_r257
    tests/test_jag_machine_jerry_timer_readback_r257.c)
  target_include_directories(test_jag_machine_jerry_timer_readback_r257 PRIVATE include)
  target_link_libraries(test_jag_machine_jerry_timer_readback_r257 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_jerry_timer_readback_r257 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_jerry_timer_readback_r257
    COMMAND test_jag_machine_jerry_timer_readback_r257)
endif()

# R258: actual Jaguar MovePlayer rotation from native AI TURN commands,
# plus original species-specific strafe and combined turn+strafe mapping.
if(UNIX AND NOT APPLE AND AVP_BUILD_LOCAL_MULTIPLAYER)
  add_test(NAME local_mp_bot_native_turn_r258 COMMAND test_local_mp_native_context)
  if(TARGET test_local_mp_native_backend)
    add_test(NAME local_mp_vent_world_presence_r272
      COMMAND test_local_mp_native_backend)
  endif()
endif()

# RE42 R259: JERRY UART register/status and double-buffered baud-time model.
# This is an isolated Jaguar reference-machine fixture, not PC networking.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_jerry_uart_r259
    tests/test_jag_machine_jerry_uart_r259.c)
  target_include_directories(test_jag_machine_jerry_uart_r259 PRIVATE include)
  target_link_libraries(test_jag_machine_jerry_uart_r259 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_jerry_uart_r259 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_jerry_uart_r259
    COMMAND test_jag_machine_jerry_uart_r259)
endif()

# RE42 R260: JERRY I2S register width and generated word-strobe gating.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_jerry_i2s_control_r260
    tests/test_jag_machine_jerry_i2s_control_r260.c)
  target_include_directories(test_jag_machine_jerry_i2s_control_r260 PRIVATE include)
  target_link_libraries(test_jag_machine_jerry_i2s_control_r260 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_jerry_i2s_control_r260 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_jerry_i2s_control_r260
    COMMAND test_jag_machine_jerry_i2s_control_r260)
endif()

if(AVP_BUILD_JAG_REFERENCE)
  add_executable(test_jag_machine_jerry_i2s_word_edges_r261
    tests/test_jag_machine_jerry_i2s_word_edges_r261.c)
  target_include_directories(test_jag_machine_jerry_i2s_word_edges_r261 PRIVATE include)
  target_link_libraries(test_jag_machine_jerry_i2s_word_edges_r261 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_jerry_i2s_word_edges_r261 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_jerry_i2s_word_edges_r261
    COMMAND test_jag_machine_jerry_i2s_word_edges_r261)
endif()

# RE43 R262: JERRY synchronous serial source 5 must reach CPU JINTCTRL.
if(AVP_BUILD_JAG_REFERENCE AND TARGET avp_jag_refmachine)
  add_executable(test_jag_machine_jerry_i2s_cpu_irq_r262
    tests/test_jag_machine_jerry_i2s_cpu_irq_r262.c)
  target_include_directories(test_jag_machine_jerry_i2s_cpu_irq_r262 PRIVATE include)
  target_link_libraries(test_jag_machine_jerry_i2s_cpu_irq_r262 PRIVATE avp_jag_refmachine)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_jag_machine_jerry_i2s_cpu_irq_r262 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME jag_machine_jerry_i2s_cpu_irq_r262
    COMMAND test_jag_machine_jerry_i2s_cpu_irq_r262)
endif()

# RE43 R263 full-sweep closure: execute the previously unregistered R153
# Blitter local-RAM regression and the three existing ROM-free Map Maker tests
# on every GCC/Clang/ASan CTest run, instead of merely compiling their targets.
if(TARGET test_jag_blit_local_ram_r153)
  add_test(NAME jag_blit_dsp_local_byte_r263 COMMAND test_jag_blit_local_ram_r153)
endif()
if(TARGET test_pc_map_maker)
  add_test(NAME pc_map_maker_core_r263 COMMAND test_pc_map_maker)
endif()
if(TARGET test_pc_map_maker_amp)
  add_test(NAME pc_map_maker_amp_r263 COMMAND test_pc_map_maker_amp)
endif()
if(TARGET test_pc_map_maker_door_flow)
  add_test(NAME pc_map_maker_door_flow_r263 COMMAND test_pc_map_maker_door_flow)
endif()

# RE43 R264 second full sweep: repair obsolete *test fixtures*, not working
# hardware, gameplay, or PC-port behavior, and keep these formerly unregistered
# ROM-free tests in GCC/Clang/ASan's routine CTest coverage.
if(TARGET test_jag_blitter_bus_provenance_r149)
  add_test(NAME jag_blitter_gpu_bus_provenance_r264
    COMMAND test_jag_blitter_bus_provenance_r149)
endif()
if(TARGET test_jag_blitter_status_r150)
  add_test(NAME jag_blitter_status_and_readback_r264
    COMMAND test_jag_blitter_status_r150)
endif()
if(TARGET test_f12_enemy_activation_r71)
  add_test(NAME f12_enemy_activation_persistence_r264
    COMMAND test_f12_enemy_activation_r71)
endif()
if(TARGET test_local_mp_player_step)
  add_test(NAME local_mp_original_player_step_r264
    COMMAND test_local_mp_player_step)
endif()
if(TARGET test_pc_prototype_textures_s105)
  add_test(NAME pc_prototype_textures_fixture_r264
    COMMAND ${CMAKE_COMMAND}
      -DTEST_BINARY=$<TARGET_FILE:test_pc_prototype_textures_s105>
      -P ${PROJECT_SOURCE_DIR}/tests/run_prototype_texture_fixture_r264.cmake)
endif()
if(UNIX AND NOT APPLE)
  find_program(AVP_R264_XVFB_RUN xvfb-run)
  if(AVP_R264_XVFB_RUN)
    if(TARGET test_pc_dm_status_direct_x11)
      add_test(NAME pc_dm_status_direct_x11_r264
        COMMAND ${AVP_R264_XVFB_RUN} -a -s "-screen 0 2560x1440x24"
          $<TARGET_FILE:test_pc_dm_status_direct_x11>)
    endif()
    if(TARGET test_local_mp_one_window_x11)
      add_test(NAME local_mp_one_window_x11_r264
        COMMAND ${AVP_R264_XVFB_RUN} -a -s "-screen 0 2560x1440x24"
          $<TARGET_FILE:test_local_mp_one_window_x11>)
    endif()
    # R265: exercise the existing idle inhibitor lifecycle with an actual X11
    # virtual screen. Test-only libdbus_shutdown frees process-global DBus
    # caches after the application's connection has closed (ASan/LSan clean).
    if(TARGET test_pc_idle_inhibit)
      add_test(NAME pc_idle_inhibit_x11_r265
        COMMAND ${AVP_R264_XVFB_RUN} -a -s "-screen 0 2560x1440x24"
          $<TARGET_FILE:test_pc_idle_inhibit>)
    endif()
  endif()
endif()

# R265: the retail cartridge image is user-supplied, NEVER distributed in the
# source/binary release. The 4 ROM-backed tests run via CTest only when the
# caller explicitly provides the exact authentic 4 MiB retail AVP.jag.
set(AVP_TEST_RETAIL_ROM "" CACHE FILEPATH
    "Optional path to your own canonical 4MiB retail AVP.jag (ROM-gated tests)")
if(AVP_TEST_RETAIL_ROM)
  if(NOT EXISTS "${AVP_TEST_RETAIL_ROM}")
    message(FATAL_ERROR "AVP_TEST_RETAIL_ROM does not exist")
  endif()
  file(SIZE "${AVP_TEST_RETAIL_ROM}" _avp_r265_rom_size)
  file(SHA256 "${AVP_TEST_RETAIL_ROM}" _avp_r265_rom_sha)
  if(NOT _avp_r265_rom_size EQUAL 4194304 OR
     NOT _avp_r265_rom_sha STREQUAL
       "b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b")
    message(FATAL_ERROR "AVP_TEST_RETAIL_ROM is not the canonical retail ROM")
  endif()
  if(TARGET test_local_mp_bot_retail_graph)
    add_test(NAME pc_retail_bot_level_graph_r265
      COMMAND $<TARGET_FILE:test_local_mp_bot_retail_graph> "${AVP_TEST_RETAIL_ROM}")
  endif()
  if(TARGET test_local_mp_retail_rom)
    add_test(NAME pc_retail_maze_collision_r265
      COMMAND $<TARGET_FILE:test_local_mp_retail_rom> "${AVP_TEST_RETAIL_ROM}")
  endif()
  if(TARGET test_re37_border_removal)
    add_test(NAME pc_retail_border_counter_r265
      COMMAND $<TARGET_FILE:test_re37_border_removal> "${AVP_TEST_RETAIL_ROM}"
        "${CMAKE_CURRENT_BINARY_DIR}/retail_border_counter_r265.csv")
  endif()
  if(TARGET test_mjp_live_cursor_rom_r93)
    add_test(NAME pc_retail_title_cursor_r265
      COMMAND $<TARGET_FILE:test_mjp_live_cursor_rom_r93> "${AVP_TEST_RETAIL_ROM}")
  endif()
  # RE44 R287: opt-in progressive Blitter must boot the authentic cartridge,
  # not merely pass ROM-free transfer tests. ROM is input only, never shipped.
  if(TARGET test_jag_blitter_bus_grant_r287)
    add_test(NAME jag_blitter_retail_boot_r287
      COMMAND $<TARGET_FILE:test_jag_blitter_bus_grant_r287> "${AVP_TEST_RETAIL_ROM}")
  endif()
  # R266: the prior full sweep incorrectly reported a retail multiplayer
  # startup stall by reading --log, while MP diagnostics write to stderr.
  # Observe real four-player ticks through combined stdout/stderr and stop
  # once the original source-driven simulation has advanced 20 ticks.
  if(UNIX AND NOT APPLE AND TARGET avp_pc AND AVP_BUILD_LOCAL_MULTIPLAYER
     AND AVP_R264_XVFB_RUN)
    find_program(AVP_R266_PYTHON3 python3)
    if(AVP_R266_PYTHON3)
      add_test(NAME pc_retail_mp_four_player_live_r266
        COMMAND "${AVP_R264_XVFB_RUN}" -a -s "-screen 0 1280x720x24"
          "${AVP_R266_PYTHON3}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pc_retail_mp_live_x11_r266.py"
          $<TARGET_FILE:avp_pc> "${AVP_TEST_RETAIL_ROM}")
      set_tests_properties(pc_retail_mp_four_player_live_r266 PROPERTIES TIMEOUT 38)
    endif()
  endif()
endif()

# RE43 R267: original End_Select does not republish the initial carousel.
# The final selected detail frame persists until the gameplay display takes over.
add_executable(test_pc_select_exit_frame_hold_r267
  tests/test_pc_select_exit_frame_hold_r267.c
  new/jaguar/avp_gpu/frontend_renderer.c
  new/jaguar/avp_gpu/frontend_fade.c
  new/jaguar/hw/cry.c
  new/jaguar/hw/framebuffer.c)
target_include_directories(test_pc_select_exit_frame_hold_r267 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_select_exit_frame_hold_r267 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_select_exit_frame_hold_r267 COMMAND test_pc_select_exit_frame_hold_r267)

# RE43 R272: multiplayer enemy-overlap exit uses the retail AMP footprint,
# without altering original single-player AMPCollisions or player physics.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_mp_enemy_overlap_escape_r272
    tests/test_collision.c src/game/collision.c)
  target_include_directories(test_mp_enemy_overlap_escape_r272 PRIVATE include)
  target_compile_options(test_mp_enemy_overlap_escape_r272 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_mp_enemy_overlap_escape_r272 PRIVATE
    -Wl,--gc-sections)
  add_test(NAME mp_enemy_overlap_escape_r272
           COMMAND test_mp_enemy_overlap_escape_r272)
endif()

if(AVP_BUILD_JAG_REFERENCE)
  add_test(NAME jag_blitter_async_r286 COMMAND test_jag_blitter_async_r286)
  add_test(NAME jag_blitter_bus_grant_r287 COMMAND test_jag_blitter_bus_grant_r287)
endif()

# RE44 R288: no-bot-stall acceptance proxy, with player collisions OFF.
if(TARGET test_r288_acid_crossing)
  add_test(NAME re44_r288_acid_crossing COMMAND test_r288_acid_crossing)
endif()

# RE45 R297: source-linked real AMP chase displacement, all quadrant directions.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_amp_diagonal_movement_r297
    tests/test_amp_diagonal_movement_r297.c tests/amp_link_stubs.c
    src/game/amp.c src/game/runtime.c)
  target_include_directories(test_amp_diagonal_movement_r297 PRIVATE include)
  target_compile_options(test_amp_diagonal_movement_r297 PRIVATE
    -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_amp_diagonal_movement_r297 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_amp_diagonal_movement_r297 PRIVATE m)
  add_test(NAME amp_diagonal_movement_r297 COMMAND test_amp_diagonal_movement_r297)
endif()

# RE45 R301: executable-relative, actual 1254x1254 RGB PNG pack regression.
add_executable(test_pc_hi_res_textures_r301
  tests/test_pc_hi_res_textures_r301.c
  new/pc_host/src/pc_hi_res_textures.c)
target_include_directories(test_pc_hi_res_textures_r301 PRIVATE include)
target_link_libraries(test_pc_hi_res_textures_r301 PRIVATE ZLIB::ZLIB)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hi_res_textures_r301 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME pc_hi_res_textures_r301
  COMMAND test_pc_hi_res_textures_r301 ${CMAKE_CURRENT_SOURCE_DIR})

# RE45 R302: keep high-resolution wall fractional sampling and translucent
# HUD ink compositing correct without touching original Jaguar gameplay.
add_executable(test_pc_hi_res_wall_hud_r302 tests/test_pc_hi_res_wall_hud_r302.c)
target_include_directories(test_pc_hi_res_wall_hud_r302 PRIVATE include)
target_link_libraries(test_pc_hi_res_wall_hud_r302 PRIVATE m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hi_res_wall_hud_r302 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hi_res_wall_hud_r302 COMMAND test_pc_hi_res_wall_hud_r302)

# R303: exact Jaguar integer automap mapping with host-only subpixel sampling.
add_executable(test_pc_hi_res_automap_r303
  tests/test_pc_hi_res_automap_r303.c
  new/jaguar/avp_gpu/automap.c)
target_include_directories(test_pc_hi_res_automap_r303 PRIVATE include)
target_link_libraries(test_pc_hi_res_automap_r303 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hi_res_automap_r303 PRIVATE
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_hi_res_automap_r303 PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME pc_hi_res_automap_r303 COMMAND test_pc_hi_res_automap_r303)

# R304 independent PC-only HUD rendering enhancements.
add_executable(test_pc_hires_hud_r304 tests/test_pc_hires_hud_r304.c)
target_include_directories(test_pc_hires_hud_r304 PRIVATE include)
target_link_libraries(test_pc_hires_hud_r304 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hires_hud_r304 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hires_hud_r304 COMMAND test_pc_hires_hud_r304)

# RE45 R306: scaled sign composites, RGBA coverage and high-resolution
# authored-window silhouette interpolation, while preserving native defaults.
add_executable(test_pc_hi_res_signs_r306
  tests/test_pc_hi_res_signs_r306.c
  new/pc_host/src/pc_hi_res_textures.c)
target_include_directories(test_pc_hi_res_signs_r306 PRIVATE include)
target_link_libraries(test_pc_hi_res_signs_r306 PRIVATE ZLIB::ZLIB m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hi_res_signs_r306 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hi_res_signs_r306 COMMAND test_pc_hi_res_signs_r306)

# R307: guard against the previous 256 MiB budget silently dropping large
# valid external PNGs (44x1254 user pack plus sign composites).
add_executable(test_pc_hi_res_pack_budget_r307
  tests/test_pc_hi_res_pack_budget_r307.c
  new/pc_host/src/pc_hi_res_textures.c)
target_include_directories(test_pc_hi_res_pack_budget_r307 PRIVATE include)
target_link_libraries(test_pc_hi_res_pack_budget_r307 PRIVATE ZLIB::ZLIB)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hi_res_pack_budget_r307 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hi_res_pack_budget_r307 COMMAND test_pc_hi_res_pack_budget_r307)

# R308: software source-space texture filtering (not a screen blur). The
# strict test covers real mip construction, anisotropic taps, RGBA, and
# borrowed co-op image identity, independent of retail-ROM availability.
add_executable(test_pc_texture_filter_r308
  tests/test_pc_texture_filter_r308.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r308 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r308 PRIVATE avp_jaguar_compat m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_filter_r308 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r308 COMMAND test_pc_texture_filter_r308)
# R309: accelerated host filtering must match the exact original R308
# pixel and transparency results across both scalar and parallel HUD paths.
add_executable(test_pc_texture_filter_r309
  tests/test_pc_texture_filter_r309.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r309 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r309 PRIVATE avp_jaguar_compat m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_filter_r309 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r309 COMMAND test_pc_texture_filter_r309)


# R310: verify zero-change Jaguar/nearest routes, exact mip-plan selection,
# and full anisotropic tap/color/coverage parity with R309 arithmetic.
add_executable(test_pc_texture_filter_r310
  tests/test_pc_texture_filter_r310.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r310 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r310 PRIVATE avp_jaguar_compat m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_filter_r310 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r310 COMMAND test_pc_texture_filter_r310)

# RE #46 R311 -- CRY16 scanline shade-LUT and exact AF-LOD plan regression.
add_executable(test_pc_texture_filter_r311
  tests/test_pc_texture_filter_r311.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r311 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r311 PRIVATE avp_jaguar_compat m)
if(AVP_STRICT)
  target_compile_options(test_pc_texture_filter_r311 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r311 COMMAND test_pc_texture_filter_r311)

# RE46 R312: exact opaque CRY16 sampler fast-path regression across
# native/Jaguar, nonpower-of-two and 1254^2 replacement texture sizes.
add_executable(test_pc_texture_filter_r312
  tests/test_pc_texture_filter_r312.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r312 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r312 PRIVATE avp_jaguar_compat m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_filter_r312 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r312 COMMAND test_pc_texture_filter_r312)

# RE46 R313: per-wall-column invariant-U AF sampling exactness.
add_executable(test_pc_texture_filter_r313
  tests/test_pc_texture_filter_r313.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r313 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r313 PRIVATE avp_jaguar_compat m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_filter_r313 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r313 COMMAND test_pc_texture_filter_r313)

# RE46 R314: floor/ceiling bound source-mip, exact AF offset parity.
add_executable(test_pc_texture_filter_r314
  tests/test_pc_texture_filter_r314.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r314 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r314 PRIVATE avp_jaguar_compat m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_filter_r314 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r314 COMMAND test_pc_texture_filter_r314)

# RE #46 R315: eliminate filtered-path-only point-coordinate work and
# share the identical nearest/filtered row shade lookup.
add_executable(test_pc_floor_filter_r315
  tests/test_pc_floor_filter_r315.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_floor_filter_r315 PRIVATE include)
target_link_libraries(test_pc_floor_filter_r315 PRIVATE avp_jaguar_compat m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_floor_filter_r315 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_floor_filter_r315 COMMAND test_pc_floor_filter_r315)


# R316: source-column mask-continuity regression for high-resolution door trim.
add_executable(test_pc_hires_window_continuity_r316
  tests/test_pc_hires_window_continuity_r316.c)
target_include_directories(test_pc_hires_window_continuity_r316 PRIVATE include)
target_link_libraries(test_pc_hires_window_continuity_r316 PRIVATE m)
if(AVP_STRICT)
  target_compile_options(test_pc_hires_window_continuity_r316 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hires_window_continuity_r316 COMMAND test_pc_hires_window_continuity_r316)

# RE #46 / R317: exact non-power-of-two source-coordinate wrap equivalence.
add_executable(test_pc_texture_filter_r317
  tests/test_pc_texture_filter_r317.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_filter_r317 PRIVATE include)
target_link_libraries(test_pc_texture_filter_r317 PRIVATE avp_jaguar_compat m)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_filter_r317 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_filter_r317 COMMAND test_pc_texture_filter_r317)

# RE #46 / R319: 2x player movement cheat, exact native input and F12 targeting.
# The speed test executes the REAL MovePlayer() with a collision-free host
# callback; the menu test uses the REAL F12 code with isolated backend stubs.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_pc_double_movement_r319
    tests/test_pc_double_movement_r319.c src/game/player.c)
  target_include_directories(test_pc_double_movement_r319 PRIVATE include)
  target_compile_options(test_pc_double_movement_r319 PRIVATE
    -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_double_movement_r319 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_pc_double_movement_r319 PRIVATE m)
  add_test(NAME pc_double_movement_r319 COMMAND test_pc_double_movement_r319)
  if(AVP_BUILD_PC)
    add_executable(test_pc_double_movement_menu_r319
      new/pc_host/tests/test_pc_double_movement_menu_r319.c
      new/pc_host/src/pc_vulkan_probe.c
      new/map_maker/map_maker.c
      new/map_maker/map_maker_scripts.c
      new/map_maker/map_maker_overhead.c
      new/map_maker/map_maker_highlight.c
      new/map_maker/map_maker_texture_browser.c
      new/map_maker/map_maker_objects.c
      new/jaguar/avp_gpu/automap.c
      new/jaguar/avp_gpu/state.c
      new/jaguar/avp_gpu/texture_filter.c
      new/pc_host/src/pc_original_cheat_input.c)
    target_include_directories(test_pc_double_movement_menu_r319 PRIVATE
      include new/multiplayer new/map_maker new/game_micro_optimization)
    target_compile_definitions(test_pc_double_movement_menu_r319 PRIVATE
      AVP_LOCAL_MULTIPLAYER=1 _POSIX_C_SOURCE=200809L)
    target_compile_options(test_pc_double_movement_menu_r319 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(test_pc_double_movement_menu_r319 PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_pc_double_movement_menu_r319 PRIVATE
      X11 m ${CMAKE_DL_LIBS} pthread)
    add_test(NAME pc_double_movement_menu_r319 COMMAND test_pc_double_movement_menu_r319)
    # R320: real Video menu, no X11 connection or GPU required for menu logic.
    add_executable(test_pc_vulkan_menu_r320
      tests/test_pc_vulkan_menu_r320.c
      new/pc_host/src/pc_vulkan_probe.c
      new/map_maker/map_maker.c
      new/map_maker/map_maker_scripts.c
      new/map_maker/map_maker_overhead.c
      new/map_maker/map_maker_highlight.c
      new/map_maker/map_maker_texture_browser.c
      new/map_maker/map_maker_objects.c
      new/jaguar/avp_gpu/automap.c
      new/jaguar/avp_gpu/state.c
      new/jaguar/avp_gpu/texture_filter.c
      new/pc_host/src/pc_original_cheat_input.c)
    target_include_directories(test_pc_vulkan_menu_r320 PRIVATE
      include new/multiplayer new/map_maker new/game_micro_optimization)
    target_compile_definitions(test_pc_vulkan_menu_r320 PRIVATE
      AVP_LOCAL_MULTIPLAYER=1 _POSIX_C_SOURCE=200809L)
    target_compile_options(test_pc_vulkan_menu_r320 PRIVATE
      -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(test_pc_vulkan_menu_r320 PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_pc_vulkan_menu_r320 PRIVATE
      X11 m ${CMAKE_DL_LIBS} pthread)
    add_test(NAME pc_vulkan_menu_r320 COMMAND test_pc_vulkan_menu_r320)
  endif()
endif()

# RE #46 R320: independently authored Vulkan discovery tests. The mock loader
# remains test-only; it is NEVER linked or packaged with the game.
if(UNIX AND NOT APPLE)
  add_executable(test_pc_vulkan_probe_r320
    tests/test_pc_vulkan_probe_r320.c new/pc_host/src/pc_vulkan_probe.c)
  target_include_directories(test_pc_vulkan_probe_r320 PRIVATE include)
  target_compile_options(test_pc_vulkan_probe_r320 PRIVATE -Wall -Wextra -Werror)
  target_link_libraries(test_pc_vulkan_probe_r320 PRIVATE ${CMAKE_DL_LIBS})
  add_test(NAME pc_vulkan_probe_r320_missing_driver COMMAND test_pc_vulkan_probe_r320)
  add_library(avp_test_vulkan_loader_r320 SHARED tests/mock_vulkan_loader_r320.c)
  set_target_properties(avp_test_vulkan_loader_r320 PROPERTIES
    OUTPUT_NAME vulkan VERSION 1.0 SOVERSION 1
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_vk_loader_r320")
  target_compile_options(avp_test_vulkan_loader_r320 PRIVATE -Wall -Wextra -Werror)
  add_test(NAME pc_vulkan_probe_r320_mock COMMAND test_pc_vulkan_probe_r320)
  set_tests_properties(pc_vulkan_probe_r320_mock PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:avp_test_vulkan_loader_r320>;AVP_EXPECT_MOCK_VULKAN=1")
endif()

# RE #46 R326: SDK-independent descriptor input validation.
add_executable(test_pc_vulkan_descriptor_policy_r326
  tests/test_pc_vulkan_descriptor_policy_r326.c
  new/pc_host/src/pc_vulkan_descriptor_policy.c)
target_include_directories(test_pc_vulkan_descriptor_policy_r326 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_descriptor_policy_r326 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_descriptor_policy_r326 COMMAND test_pc_vulkan_descriptor_policy_r326)

# R327: validate bounded SPIR-V modules even on systems with no Vulkan SDK.
add_executable(test_pc_vulkan_spirv_contract_r327
  tests/test_pc_vulkan_spirv_contract_r327.c
  new/pc_host/src/pc_vulkan_spirv_contract.c)
target_include_directories(test_pc_vulkan_spirv_contract_r327 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_spirv_contract_r327 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_spirv_contract_r327 COMMAND test_pc_vulkan_spirv_contract_r327)

# R330: shader output validation runs without Vulkan SDK or artwork.
add_executable(test_pc_vulkan_readback_r330
  tests/test_pc_vulkan_readback_r330.c
  new/pc_host/src/pc_vulkan_readback_policy.c)
target_include_directories(test_pc_vulkan_readback_r330 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_readback_r330 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_readback_r330 COMMAND test_pc_vulkan_readback_r330)

option(AVP_BUILD_VULKAN_SHADER_DIAGNOSTIC
  "Build synthetic GLSL shaders for opt-in Vulkan WSI draw diagnostic" OFF)
if(AVP_BUILD_VULKAN_SHADER_DIAGNOSTIC AND NOT AVP_BUILD_VULKAN_WSI_DIAGNOSTIC)
  message(FATAL_ERROR "Shader diagnostic requires AVP_BUILD_VULKAN_WSI_DIAGNOSTIC=ON")
endif()

# RE #46 R321: first real GPU-owned image/swapchain clear diagnostic.
# This is deliberately a standalone executable, NOT the game's active backend.
# The system Vulkan SDK is mandatory when explicitly requested. No loader,
# driver or Vulkan headers are redistributed in game archives.
option(AVP_BUILD_VULKAN_WSI_DIAGNOSTIC
    "Build the standalone Vulkan Xlib device/swapchain/GPU-clear diagnostic" OFF)
if(AVP_BUILD_VULKAN_WSI_DIAGNOSTIC)
  if(NOT UNIX OR APPLE OR WIN32)
    message(FATAL_ERROR "R321 Vulkan WSI diagnostic supports Linux/X11 only")
  endif()
  find_package(Vulkan REQUIRED)
  # R378 full sweep: historical Vulkan mock targets compile SDK headers but
  # intentionally do not link libvulkan (their Vulkan calls are mocked).
  # Make the located SDK's include path available to those test targets too.
  include_directories(SYSTEM ${Vulkan_INCLUDE_DIRS})
  find_package(X11 REQUIRED)
  add_executable(avp_vulkan_wsi_smoke
      tests/avp_vulkan_wsi_smoke_main.c
      new/pc_host/src/pc_vulkan_wsi.c
      new/pc_host/src/pc_hybrid_pipeline_cache.c
      new/pc_host/src/pc_hybrid_shader_cache.c
      new/pc_host/src/pc_hybrid_gpu_parity.c
      new/pc_host/src/pc_hybrid_stream_policy.c
      new/pc_host/src/pc_vulkan_hybrid_stream.c
      new/pc_host/src/pc_hybrid_texture_cache.c
      new/pc_host/src/pc_vulkan_wsi_policy.c
      new/pc_host/src/pc_vulkan_frame_policy.c
      new/pc_host/src/pc_vulkan_upload.c
      new/pc_host/src/pc_vulkan_upload_policy.c
      new/pc_host/src/pc_vulkan_descriptors.c
      new/pc_host/src/pc_vulkan_descriptor_policy.c
      new/pc_host/src/pc_vulkan_spirv_contract.c
      new/pc_host/src/pc_vulkan_opaque_pipeline.c
      new/pc_host/src/pc_vulkan_readback.c
      new/pc_host/src/pc_vulkan_wall_oracle_readback.c
      new/pc_host/src/pc_vulkan_wall_pixel_oracle.c
      new/pc_host/src/pc_vulkan_wall_oracle_capture.c
      new/pc_host/src/pc_vulkan_wall_mixed_fixture.c
      new/pc_host/src/pc_vulkan_wall_live_frame.c
      new/pc_host/src/pc_vulkan_flat_frame.c
      new/pc_host/src/pc_vulkan_flat_projection.c
      new/pc_host/src/pc_vulkan_wall_layers.c
      new/pc_host/src/pc_vulkan_layered_frame.c
      new/pc_host/src/pc_hybrid_viewport.c
      new/pc_host/src/pc_vulkan_wall_lit_stream.c
      new/pc_host/src/pc_vulkan_wall_lighting.cpp
      new/jaguar/avp_gpu/projection.cpp
      new/pc_host/src/pc_vulkan_retail_shade_bridge.cpp
      new/pc_host/src/pc_vulkan_readback_policy.c
      new/pc_host/src/pc_vulkan_geometry_upload.c
      new/pc_host/src/pc_vulkan_geometry_policy.c
      new/pc_host/src/pc_vulkan_ray_columns.c
      new/pc_host/src/pc_vulkan_wall_projection.c
      new/pc_host/src/pc_vulkan_wall_slots.c
      new/pc_host/src/pc_vulkan_wall_source_upload.c
      new/pc_host/src/pc_vulkan_wall_image_bank.c
      new/pc_host/src/pc_vulkan_wall_draw_batch.c
      new/pc_host/src/pc_vulkan_wall_gpu_bank.c
      new/jaguar/avp_gpu/world_texture_transfer.c
      new/jaguar/avp_gpu/world_scene_packet.c
      new/jaguar/hw/cry.c
      new/jaguar/avp_gpu/maze2_column.c)
  target_include_directories(avp_vulkan_wsi_smoke PRIVATE
      include ${X11_INCLUDE_DIR})
  # R339's image bank reuses R324's world_scene_pack_image, not its
  # unrelated CPU scene capture. Dead-strip the unreferenced legacy scene
  # entrypoints rather than linking the entire Software gameplay engine.
  target_compile_options(avp_vulkan_wsi_smoke PRIVATE
      -Wall -Wextra -Werror -ffunction-sections -fdata-sections)
  target_link_options(avp_vulkan_wsi_smoke PRIVATE -Wl,--gc-sections)
  target_link_libraries(avp_vulkan_wsi_smoke PRIVATE
      Vulkan::Vulkan ${X11_LIBRARIES})
  # Distribution binaries resolve libvulkan.so.1 from the destination's
  # installed Vulkan loader, not the build host's private SDK directory.
  set_target_properties(avp_vulkan_wsi_smoke PROPERTIES SKIP_BUILD_RPATH TRUE)
  if(AVP_BUILD_VULKAN_SHADER_DIAGNOSTIC)
    find_program(AVP_GLSLANG_VALIDATOR NAMES glslangValidator REQUIRED)
    set(AVP_SHADER_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/shaders")
    set(AVP_SHADER_OUTPUT "${AVP_PORT_BINARY_ROOT}/avp_vk_shader_r327")
    set(AVP_VERTEX_SPV "${AVP_SHADER_OUTPUT}/avp_diag_opaque.vert.spv")
    set(AVP_FRAGMENT_SPV "${AVP_SHADER_OUTPUT}/avp_diag_opaque.frag.spv")
    set(AVP_RAY_FRAGMENT_SPV "${AVP_SHADER_OUTPUT}/avp_diag_ray_read.frag.spv")
    set(AVP_COLUMNS_FRAGMENT_SPV "${AVP_SHADER_OUTPUT}/avp_diag_ray_columns.frag.spv")
    add_custom_command(OUTPUT "${AVP_VERTEX_SPV}" "${AVP_FRAGMENT_SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        -S vert -o "${AVP_VERTEX_SPV}" "${AVP_SHADER_FOLDER}/avp_diag_opaque.vert"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_FRAGMENT_SPV}"
        "${AVP_SHADER_FOLDER}/avp_diag_opaque.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_diag_opaque.vert"
              "${AVP_SHADER_FOLDER}/avp_diag_opaque.frag"
              "${AVP_SHADER_FOLDER}/avp_world_opaque_integer.glsl"
      VERBATIM)
    add_custom_command(OUTPUT "${AVP_RAY_FRAGMENT_SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_RAY_FRAGMENT_SPV}"
        "${AVP_SHADER_FOLDER}/avp_diag_ray_read.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_diag_ray_read.frag"
              "${AVP_SHADER_FOLDER}/avp_world_opaque_integer.glsl"
      VERBATIM)
    add_custom_command(OUTPUT "${AVP_COLUMNS_FRAGMENT_SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_COLUMNS_FRAGMENT_SPV}"
        "${AVP_SHADER_FOLDER}/avp_diag_ray_columns.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_diag_ray_columns.frag"
              "${AVP_SHADER_FOLDER}/avp_world_opaque_integer.glsl"
      VERBATIM)
    # R336: first source-derived projected *solid* wall source-texel shader.
    # Compiled only when user explicitly supplies official SDK + glslang.
    set(AVP_WALL_FRAGMENT_SPV "${AVP_SHADER_OUTPUT}/avp_diag_projected_wall.frag.spv")
    add_custom_command(OUTPUT "${AVP_WALL_FRAGMENT_SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_WALL_FRAGMENT_SPV}"
        "${AVP_SHADER_FOLDER}/avp_diag_projected_wall.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_diag_projected_wall.frag"
              "${AVP_SHADER_FOLDER}/avp_world_opaque_integer.glsl"
      VERBATIM)
    add_custom_target(avp_vk_projected_wall_shader_r336
      DEPENDS "${AVP_WALL_FRAGMENT_SPV}")
    # R337 optional two-bound-source-image wall shader diagnostic.
    set(AVP_TWO_SLOT_FRAGMENT_SPV "${AVP_SHADER_OUTPUT}/avp_diag_two_slot_wall.frag.spv")
    add_custom_command(OUTPUT "${AVP_TWO_SLOT_FRAGMENT_SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_TWO_SLOT_FRAGMENT_SPV}"
        "${AVP_SHADER_FOLDER}/avp_diag_two_slot_wall.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_diag_two_slot_wall.frag"
              "${AVP_SHADER_FOLDER}/avp_world_opaque_integer.glsl"
      VERBATIM)
    add_custom_target(avp_vk_two_slot_wall_shader_r337
      DEPENDS "${AVP_TWO_SLOT_FRAGMENT_SPV}")
    # R355: exact original floor/ceiling texel projection fragment pass.
    # Compiles only with the Vulkan SDK. Not bound to the gameplay pipeline.
    set(AVP_FLAT_FRAGMENT_SPV "${AVP_SHADER_OUTPUT}/avp_diag_flat.frag.spv")
    add_custom_command(OUTPUT "${AVP_FLAT_FRAGMENT_SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_FLAT_FRAGMENT_SPV}"
        "${AVP_SHADER_FOLDER}/avp_diag_flat.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_diag_flat.frag"
              "${AVP_SHADER_FOLDER}/avp_world_opaque_integer.glsl"
      VERBATIM)
    add_custom_target(avp_vk_flat_shader_r355
      DEPENDS "${AVP_FLAT_FRAGMENT_SPV}")
    # R360: exact final-XRGB actor/weapon/HUD foreground compositor shader.
    # Shader is not enabled in the game until the continuous GPU frame path
    # can present world + foreground atomically, with Software fallback.
    set(AVP_HYBRID_FOREGROUND_SPV
        "${AVP_SHADER_OUTPUT}/avp_hybrid_foreground.frag.spv")
    add_custom_command(OUTPUT "${AVP_HYBRID_FOREGROUND_SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        -S frag -o "${AVP_HYBRID_FOREGROUND_SPV}"
        "${AVP_SHADER_FOLDER}/avp_hybrid_foreground.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_hybrid_foreground.frag"
      VERBATIM)
    add_custom_target(avp_vk_hybrid_foreground_shader_r360
      DEPENDS "${AVP_HYBRID_FOREGROUND_SPV}")
    # R361: native image UV mapping and two-pass game-window Hybrid Vulkan.
    # Compiled only for the explicitly enabled Vulkan SDK game target.
    set(AVP_HYBRID_GAME_WORLD_FS
        "${AVP_SHADER_OUTPUT}/avp_hybrid_world.frag.spv")
    set(AVP_HYBRID_GAME_OVERLAY_FS
        "${AVP_SHADER_OUTPUT}/avp_hybrid_present_overlay.frag.spv")
    add_custom_command(OUTPUT "${AVP_HYBRID_GAME_WORLD_FS}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_HYBRID_GAME_WORLD_FS}"
        "${AVP_SHADER_FOLDER}/avp_hybrid_world.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_hybrid_world.frag"
              "${AVP_SHADER_FOLDER}/avp_scanlines.glsl"
              "${AVP_SHADER_FOLDER}/avp_world_opaque_integer.glsl"
      VERBATIM)
    add_custom_command(OUTPUT "${AVP_HYBRID_GAME_OVERLAY_FS}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${AVP_SHADER_OUTPUT}"
      COMMAND "${AVP_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
        "-I${AVP_SHADER_FOLDER}" -S frag -o "${AVP_HYBRID_GAME_OVERLAY_FS}"
        "${AVP_SHADER_FOLDER}/avp_hybrid_present_overlay.frag"
      DEPENDS "${AVP_SHADER_FOLDER}/avp_hybrid_present_overlay.frag"
              "${AVP_SHADER_FOLDER}/avp_scanlines.glsl"
      VERBATIM)
    add_custom_target(avp_vk_hybrid_present_shader_r361 DEPENDS
      "${AVP_HYBRID_GAME_WORLD_FS}" "${AVP_HYBRID_GAME_OVERLAY_FS}")
    add_custom_target(avp_vk_shaders_r327 ALL DEPENDS
      "${AVP_VERTEX_SPV}" "${AVP_FRAGMENT_SPV}" "${AVP_RAY_FRAGMENT_SPV}"
      "${AVP_COLUMNS_FRAGMENT_SPV}")
    add_dependencies(avp_vulkan_wsi_smoke avp_vk_shaders_r327)
    if(BUILD_TESTING)
      add_test(NAME pc_vulkan_shader_draw_r327 COMMAND avp_vulkan_wsi_smoke
        --shader-test "${AVP_VERTEX_SPV}" "${AVP_FRAGMENT_SPV}")
      set_tests_properties(pc_vulkan_shader_draw_r327 PROPERTIES SKIP_RETURN_CODE 77)
      add_test(NAME pc_vulkan_shader_ray_read_r333 COMMAND avp_vulkan_wsi_smoke
        --shader-test "${AVP_VERTEX_SPV}" "${AVP_RAY_FRAGMENT_SPV}")
      set_tests_properties(pc_vulkan_shader_ray_read_r333 PROPERTIES SKIP_RETURN_CODE 77)
      add_test(NAME pc_vulkan_shader_ray_columns_r335 COMMAND avp_vulkan_wsi_smoke
        --ray-columns "${AVP_VERTEX_SPV}" "${AVP_COLUMNS_FRAGMENT_SPV}")
      set_tests_properties(pc_vulkan_shader_ray_columns_r335 PROPERTIES SKIP_RETURN_CODE 77)
      add_test(NAME pc_vulkan_shader_projected_wall_r336 COMMAND avp_vulkan_wsi_smoke
        --projected-wall "${AVP_VERTEX_SPV}" "${AVP_WALL_FRAGMENT_SPV}")
      set_tests_properties(pc_vulkan_shader_projected_wall_r336 PROPERTIES SKIP_RETURN_CODE 77)
      add_test(NAME pc_vulkan_shader_two_slot_wall_r337 COMMAND avp_vulkan_wsi_smoke
        --two-slot-wall "${AVP_VERTEX_SPV}" "${AVP_TWO_SLOT_FRAGMENT_SPV}")
      set_tests_properties(pc_vulkan_shader_two_slot_wall_r337 PROPERTIES SKIP_RETURN_CODE 77)
      add_test(NAME pc_vulkan_shader_mixed_cry_rgb_r348 COMMAND avp_vulkan_wsi_smoke
        --mixed-cry-rgb-wall "${AVP_VERTEX_SPV}" "${AVP_TWO_SLOT_FRAGMENT_SPV}")
      set_tests_properties(pc_vulkan_shader_mixed_cry_rgb_r348 PROPERTIES SKIP_RETURN_CODE 77)
    endif()
  endif()
  if(BUILD_TESTING)
    add_executable(test_pc_vulkan_upload_mock_r325
      tests/test_pc_vulkan_upload_mock_r325.c
      new/pc_host/src/pc_vulkan_upload.c
      new/pc_host/src/pc_vulkan_upload_policy.c)
    target_include_directories(test_pc_vulkan_upload_mock_r325 PRIVATE include ${Vulkan_INCLUDE_DIRS})
    target_compile_options(test_pc_vulkan_upload_mock_r325 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME pc_vulkan_upload_mock_r325 COMMAND test_pc_vulkan_upload_mock_r325)
    add_executable(test_pc_vulkan_geometry_mock_r332
      tests/test_pc_vulkan_geometry_mock_r332.c
      new/pc_host/src/pc_vulkan_geometry_upload.c
      new/pc_host/src/pc_vulkan_geometry_policy.c
      new/pc_host/src/pc_vulkan_wall_projection.c
      new/jaguar/avp_gpu/maze2_column.c)
    target_include_directories(test_pc_vulkan_geometry_mock_r332 PRIVATE include)
    target_link_libraries(test_pc_vulkan_geometry_mock_r332 PRIVATE m)
    target_compile_options(test_pc_vulkan_geometry_mock_r332 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME pc_vulkan_geometry_mock_r332
      COMMAND test_pc_vulkan_geometry_mock_r332)
    add_executable(test_pc_vulkan_readback_mock_r330
      tests/test_pc_vulkan_readback_mock_r330.c
      new/pc_host/src/pc_vulkan_readback.c
      new/pc_host/src/pc_vulkan_readback_policy.c
      new/pc_host/src/pc_vulkan_wall_projection.c
      new/jaguar/avp_gpu/maze2_column.c)
    target_include_directories(test_pc_vulkan_readback_mock_r330 PRIVATE include)
    target_link_libraries(test_pc_vulkan_readback_mock_r330 PRIVATE m)
    target_compile_options(test_pc_vulkan_readback_mock_r330 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME pc_vulkan_readback_mock_r330
      COMMAND test_pc_vulkan_readback_mock_r330)
    add_executable(test_pc_vulkan_descriptors_mock_r326
      tests/test_pc_vulkan_descriptors_mock_r326.c
      new/pc_host/src/pc_vulkan_descriptors.c
      new/pc_host/src/pc_vulkan_descriptor_policy.c)
    target_include_directories(test_pc_vulkan_descriptors_mock_r326 PRIVATE include)
    target_compile_options(test_pc_vulkan_descriptors_mock_r326 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME pc_vulkan_descriptors_mock_r326
      COMMAND test_pc_vulkan_descriptors_mock_r326)
    # R333: optional official-SDK mock test exercises image+ray-storage
    # descriptors and allocation failure recovery (no GPU is emulated).
    add_executable(test_pc_vulkan_descriptors_mock_r333
      tests/test_pc_vulkan_descriptors_mock_r333.c
      new/pc_host/src/pc_vulkan_descriptors.c
      new/pc_host/src/pc_vulkan_descriptor_policy.c)
    target_include_directories(test_pc_vulkan_descriptors_mock_r333 PRIVATE include)
    target_compile_options(test_pc_vulkan_descriptors_mock_r333 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME pc_vulkan_descriptors_mock_r333
      COMMAND test_pc_vulkan_descriptors_mock_r333)
    add_executable(test_pc_vulkan_pipeline_mock_r327
      tests/test_pc_vulkan_pipeline_mock_r327.c
      new/pc_host/src/pc_vulkan_opaque_pipeline.c
      new/pc_host/src/pc_vulkan_spirv_contract.c)
    target_include_directories(test_pc_vulkan_pipeline_mock_r327 PRIVATE include)
    target_compile_options(test_pc_vulkan_pipeline_mock_r327 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME pc_vulkan_pipeline_mock_r327 COMMAND test_pc_vulkan_pipeline_mock_r327)

    add_test(NAME pc_vulkan_wsi_smoke_r321 COMMAND avp_vulkan_wsi_smoke)
    set_tests_properties(pc_vulkan_wsi_smoke_r321 PROPERTIES SKIP_RETURN_CODE 77)
  endif()
endif()

# R321: pure WSI capability-policy tests, runnable without Vulkan SDK/driver.
add_executable(test_pc_vulkan_wsi_policy_r321
    tests/test_pc_vulkan_wsi_policy_r321.c
    new/pc_host/src/pc_vulkan_wsi_policy.c)
target_include_directories(test_pc_vulkan_wsi_policy_r321 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wsi_policy_r321 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wsi_policy_r321 COMMAND test_pc_vulkan_wsi_policy_r321)

# R349: strict explicit GPU identity + real shader descriptor/image limits.
# Uses the SAME SDK-independent policy object as the optional Vulkan WSI.
add_executable(test_pc_vulkan_wsi_gpu_caps_r349
  tests/test_pc_vulkan_wsi_gpu_caps_r349.c
  new/pc_host/src/pc_vulkan_wsi_policy.c)
target_include_directories(test_pc_vulkan_wsi_gpu_caps_r349 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wsi_gpu_caps_r349 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wsi_gpu_caps_r349 COMMAND test_pc_vulkan_wsi_gpu_caps_r349)

# R322: portable multi-frame WSI status/resize/retry policy test; available
# without the optional Vulkan SDK or a compatible graphics adapter.
add_executable(test_pc_vulkan_frame_policy_r322
    tests/test_pc_vulkan_frame_policy_r322.c
    new/pc_host/src/pc_vulkan_frame_policy.c)
target_include_directories(test_pc_vulkan_frame_policy_r322 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_frame_policy_r322 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_frame_policy_r322 COMMAND test_pc_vulkan_frame_policy_r322)


# RE #46 R323: backend-neutral SOURCE-GEOMETRY packet, independent of SDK/GPU.
# The production Software entry point remains unchanged. This test does not
# embed original art or depend on a Vulkan driver.
add_executable(test_pc_vulkan_scene_packet_r323
    tests/test_pc_vulkan_scene_packet_r323.c
    new/jaguar/avp_gpu/world_scene_packet.c)
target_include_directories(test_pc_vulkan_scene_packet_r323 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_scene_packet_r323 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
endif()
if(NOT WIN32)
  target_link_libraries(test_pc_vulkan_scene_packet_r323 PRIVATE m)
endif()
add_test(NAME pc_vulkan_scene_packet_r323 COMMAND test_pc_vulkan_scene_packet_r323)

# RE46 R324 — host-to-Vulkan R32_UINT texture staging + CRY LUT contract.
# No Vulkan SDK or GPU required. Actual shader execution is not yet active.
add_executable(test_pc_vulkan_texture_transfer_r324
    tests/test_pc_vulkan_texture_transfer_r324.c
    new/jaguar/avp_gpu/world_scene_packet.c
    new/jaguar/avp_gpu/world_texture_transfer.c
    new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_texture_transfer_r324 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_texture_transfer_r324 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
if(NOT WIN32)
  target_link_libraries(test_pc_vulkan_texture_transfer_r324 PRIVATE m)
endif()
add_test(NAME pc_vulkan_texture_transfer_r324 COMMAND test_pc_vulkan_texture_transfer_r324)

# RE46 R325 -- actual Vulkan image transfer plans; this test needs NO SDK.
add_executable(test_pc_vulkan_upload_policy_r325
  tests/test_pc_vulkan_upload_policy_r325.c
  new/pc_host/src/pc_vulkan_upload_policy.c)
target_include_directories(test_pc_vulkan_upload_policy_r325 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_upload_policy_r325 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_upload_policy_r325 COMMAND test_pc_vulkan_upload_policy_r325)

# RE46 R328: external, optional ROM-ID sprite/HUD PNG folders, no assets bundled.
add_executable(test_pc_hi_res_sprite_folders_r328
  tests/test_pc_hi_res_sprite_folders_r328.c
  new/pc_host/src/pc_hi_res_sprites.c new/pc_host/src/pc_hi_res_textures.c)
target_include_directories(test_pc_hi_res_sprite_folders_r328 PRIVATE include)
target_link_libraries(test_pc_hi_res_sprite_folders_r328 PRIVATE ZLIB::ZLIB)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hi_res_sprite_folders_r328 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hi_res_sprite_folders_r328 COMMAND test_pc_hi_res_sprite_folders_r328)

# RE46 R329: arbitrary RGBA dimensions for first-person FILES.DAT weapons.
add_executable(test_pc_hi_res_weapon_dimensions_r329
  tests/test_pc_hi_res_weapon_dimensions_r329.c
  new/jaguar/avp_gpu/fp_overlay.c)
target_include_directories(test_pc_hi_res_weapon_dimensions_r329 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hi_res_weapon_dimensions_r329 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hi_res_weapon_dimensions_r329 COMMAND test_pc_hi_res_weapon_dimensions_r329)

# RE46 R331: fixed-layout column/ray stream for eventual Vulkan storage buffer.
# The serializer is dormant in Software gameplay and contains no ROM assets.
add_executable(test_pc_vulkan_geometry_wire_r331
  tests/test_pc_vulkan_geometry_wire_r331.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/world_scene_gpu_wire.c)
target_include_directories(test_pc_vulkan_geometry_wire_r331 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_geometry_wire_r331 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
if(NOT WIN32)
  target_link_libraries(test_pc_vulkan_geometry_wire_r331 PRIVATE m)
endif()
add_test(NAME pc_vulkan_geometry_wire_r331 COMMAND test_pc_vulkan_geometry_wire_r331)

# RE46 R332 — validate exact R331 geometry GPU-buffer payload with no SDK.
# R335: synthetic full-width GPU-ray diagnostic, SDK-independent oracle.
add_executable(test_pc_vulkan_ray_columns_r335
    tests/test_pc_vulkan_ray_columns_r335.c
    new/pc_host/src/pc_vulkan_ray_columns.c
    new/pc_host/src/pc_vulkan_geometry_policy.c
    new/pc_host/src/pc_vulkan_readback_policy.c)
target_include_directories(test_pc_vulkan_ray_columns_r335 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_ray_columns_r335 PRIVATE -Wall -Wextra -Werror)
endif()
target_link_libraries(test_pc_vulkan_ray_columns_r335 PRIVATE m)
add_test(NAME pc_vulkan_ray_columns_r335 COMMAND test_pc_vulkan_ray_columns_r335)

add_executable(test_pc_vulkan_geometry_policy_r332
  tests/test_pc_vulkan_geometry_policy_r332.c
  new/pc_host/src/pc_vulkan_geometry_policy.c)
target_include_directories(test_pc_vulkan_geometry_policy_r332 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_geometry_policy_r332 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_geometry_policy_r332 COMMAND test_pc_vulkan_geometry_policy_r332)

# RE46 R334: render-free visibility capture parity, using only original raycast
# and portal-clipping functions, without allocating a software framebuffer.
if(UNIX AND NOT APPLE)
  add_executable(test_pc_vulkan_ray_only_r334
    tests/test_pc_vulkan_ray_only_r334.c
  new/pc_host/src/pc_vulkan_direct_rays.c
    new/pc_host/src/pc_vulkan_wall_projection.c
    new/jaguar/avp_gpu/world_scene_gpu_wire.c
    new/pc_host/src/pc_vulkan_geometry_policy.c
    new/jaguar/avp_gpu/texture_filter.c
    new/jaguar/avp_gpu/world_scene_packet.c
    new/jaguar/avp_gpu/maze2_column.c
    new/jaguar/avp_gpu/presentation_interp.c)
  target_include_directories(test_pc_vulkan_ray_only_r334 PRIVATE include)
  target_compile_options(test_pc_vulkan_ray_only_r334 PRIVATE
    -D_GNU_SOURCE -ffunction-sections -fdata-sections
    -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_pc_vulkan_ray_only_r334 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_pc_vulkan_ray_only_r334 PRIVATE m)
  add_test(NAME pc_vulkan_ray_only_r334 COMMAND test_pc_vulkan_ray_only_r334)
endif()

# RE46 R336: source-derived projected solid wall contract.
# Standalone SDK-free verification, no active GPU gameplay backend.
add_executable(test_pc_vulkan_wall_projection_r336
  tests/test_pc_vulkan_wall_projection_r336.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_projection_r336 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_projection_r336 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_projection_r336 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_projection_r336
  COMMAND test_pc_vulkan_wall_projection_r336)

# RE47 R337 — deterministic per-slot wall multi-draw plan, SDK-independent.
add_executable(test_pc_vulkan_wall_slots_r337
  tests/test_pc_vulkan_wall_slots_r337.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_slots_r337 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_slots_r337 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_slots_r337 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_slots_r337 COMMAND test_pc_vulkan_wall_slots_r337)

# RE47 R338: source-derived 92-slot wall image upload staging (no SDK).
# GPU resources remain provisional: downstream callers commit only on success.
add_executable(test_pc_vulkan_wall_source_upload_r338
  tests/test_pc_vulkan_wall_source_upload_r338.c
  new/pc_host/src/pc_vulkan_wall_source_upload.c
  new/jaguar/avp_gpu/world_texture_transfer.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/maze2_column.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_wall_source_upload_r338 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_source_upload_r338 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_source_upload_r338 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  if(UNIX AND NOT APPLE)
    target_link_options(test_pc_vulkan_wall_source_upload_r338 PRIVATE -Wl,--gc-sections)
  endif()
endif()
add_test(NAME pc_vulkan_wall_source_upload_r338
  COMMAND test_pc_vulkan_wall_source_upload_r338)

# RE47 R339: transactional original-source -> GPU R32_UINT image bank.
# CPU-only tests run without Vulkan headers/driver or ROM. A mock backend
# proves all-or-none ownership, including deliberate mid-upload failure.
add_executable(test_pc_vulkan_wall_image_bank_r339
  tests/test_pc_vulkan_wall_image_bank_r339.c
  new/pc_host/src/pc_vulkan_wall_image_bank.c
  new/pc_host/src/pc_vulkan_wall_source_upload.c
  new/jaguar/avp_gpu/world_texture_transfer.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_wall_image_bank_r339 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_image_bank_r339 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  if(UNIX AND NOT APPLE)
    target_link_options(test_pc_vulkan_wall_image_bank_r339 PRIVATE
      -Wl,--gc-sections)
  endif()
endif()
add_test(NAME pc_vulkan_wall_image_bank_r339
  COMMAND test_pc_vulkan_wall_image_bank_r339)

# RE47 R340: source-derived projected-wall frame -> committed image bank draw
# schedule. No Vulkan SDK and no ROM required; production game unchanged.
add_executable(test_pc_vulkan_wall_draw_batch_r340
  tests/test_pc_vulkan_wall_draw_batch_r340.c
  new/pc_host/src/pc_vulkan_wall_draw_batch.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_draw_batch_r340 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_draw_batch_r340 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_draw_batch_r340 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_draw_batch_r340
  COMMAND test_pc_vulkan_wall_draw_batch_r340)

# RE47 R340: actual Jaguar/hi-res game state sources and original ray packet,
# not synthetic texture metadata, driving the transactional render bridge.
add_executable(test_pc_vulkan_wall_live_frame_r340
  new/pc_host/src/pc_vulkan_wall_lit_stream.c
  new/pc_host/src/pc_vulkan_wall_lighting.cpp
  new/jaguar/avp_gpu/projection.cpp
  tests/test_pc_vulkan_wall_live_frame_r340.c
  new/pc_host/src/pc_vulkan_wall_live_frame.c
  new/pc_host/src/pc_vulkan_wall_draw_batch.c
  new/pc_host/src/pc_vulkan_wall_image_bank.c
  new/pc_host/src/pc_vulkan_wall_source_upload.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/world_texture_transfer.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/maze2_column.c
  new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_wall_live_frame_r340 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_live_frame_r340 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_live_frame_r340 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_pc_vulkan_wall_live_frame_r340 PRIVATE
    -Wl,--gc-sections)
endif()
add_test(NAME pc_vulkan_wall_live_frame_r340
  COMMAND test_pc_vulkan_wall_live_frame_r340)
# R350: the new optional Vulkan WSI live-wall path uses the exact same
# source/geometry preflight, independently registered for future full sweeps.
add_test(NAME pc_vulkan_live_wall_prepare_r350
  COMMAND test_pc_vulkan_wall_live_frame_r340 --prepare-only)

# RE47 R341: original Jaguar per-wall CRY intensity transported in a separate
# v2 48-byte/column wire. V1 diagnostics remain binary-compatible.
add_executable(test_pc_vulkan_wall_lighting_r341
  tests/test_pc_vulkan_wall_lighting_r341.cpp
  new/pc_host/src/pc_vulkan_wall_lighting.cpp
  new/pc_host/src/pc_vulkan_wall_lit_stream.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/jaguar/avp_gpu/maze2_column.c
  new/jaguar/avp_gpu/projection.cpp
  new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_wall_lighting_r341 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_lighting_r341 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_lighting_r341 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_lighting_r341 COMMAND test_pc_vulkan_wall_lighting_r341)

# RE47 R343: fail-closed mixed-encoding wall pass and clipped-band bounds.
# Source-order check is ONLY a CPU/static regression, NOT shader or GPU parity.
add_executable(test_pc_vulkan_wall_mixed_encoding_r343
  tests/test_pc_vulkan_wall_mixed_encoding_r343.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_mixed_encoding_r343 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_mixed_encoding_r343 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_mixed_encoding_r343 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_mixed_encoding_r343
  COMMAND test_pc_vulkan_wall_mixed_encoding_r343
    "${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/shaders/avp_diag_two_slot_wall.frag")

# RE47 R344: host-vs-shader high-resolution texel row parity and filter gate.
# CPU-only contract; this does not certify a GPU, shader compiler or FPS.
add_executable(test_pc_vulkan_wall_sampling_parity_r344
  tests/test_pc_vulkan_wall_sampling_parity_r344.c
  new/pc_host/src/pc_vulkan_wall_lit_stream.c
  new/pc_host/src/pc_vulkan_wall_lighting.cpp
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/projection.cpp
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_sampling_parity_r344 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_sampling_parity_r344 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_sampling_parity_r344 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_sampling_parity_r344
  COMMAND test_pc_vulkan_wall_sampling_parity_r344)

# RE47 R345: bounded exact per-column high-resolution row maps. GPU shader
# is a separate opt-in diagnostic, so this test runs without the Vulkan SDK.
add_executable(test_pc_vulkan_wall_exact_rows_r345
  tests/test_pc_vulkan_wall_exact_rows_r345.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_exact_rows_r345 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_exact_rows_r345 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_exact_rows_r345 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_exact_rows_r345
  COMMAND test_pc_vulkan_wall_exact_rows_r345)

# RE47 R346: source-backed CPU pixel reference for future GPU readback.
# Validates mixed RGB/CRY, per-column shade, v3 exact rows and shader
# texture-pass ownership without the SDK. Real GPU parity remains separate.
add_executable(test_pc_vulkan_wall_pixel_oracle_r346
  tests/test_pc_vulkan_wall_pixel_oracle_r346.c
  new/pc_host/src/pc_vulkan_wall_pixel_oracle.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_pixel_oracle_r346 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_pixel_oracle_r346 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_pixel_oracle_r346 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_vulkan_wall_pixel_oracle_r346
  COMMAND test_pc_vulkan_wall_pixel_oracle_r346
    "${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/shaders/avp_diag_projected_wall.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/shaders/avp_diag_two_slot_wall.frag")

# RE47 R347: retain the EXACT source words/geometry uploaded for an isolated
# Vulkan diagnostic frame and compare real readback to the R346 source oracle.
# This CPU test intentionally has no Vulkan SDK dependency.
add_executable(test_pc_vulkan_wall_oracle_capture_r347
  tests/test_pc_vulkan_wall_oracle_capture_r347.c
  new/pc_host/src/pc_vulkan_wall_oracle_capture.c
  new/pc_host/src/pc_vulkan_wall_pixel_oracle.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/world_texture_transfer.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/maze2_column.c
  new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_wall_oracle_capture_r347 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_oracle_capture_r347 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_oracle_capture_r347 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  if(UNIX AND NOT APPLE)
    target_link_options(test_pc_vulkan_wall_oracle_capture_r347 PRIVATE
      -Wl,--gc-sections)
  endif()
endif()
add_test(NAME pc_vulkan_wall_oracle_capture_r347
  COMMAND test_pc_vulkan_wall_oracle_capture_r347)

# RE47 R348: GPU-exercisable Jaguar CRY + RGB hi-res exact-row fixture.
# Its CPU regression runs on builds without Vulkan SDK or a GPU.
add_executable(test_pc_vulkan_wall_mixed_fixture_r348
  tests/test_pc_vulkan_wall_mixed_fixture_r348.c
  new/pc_host/src/pc_vulkan_wall_mixed_fixture.c
  new/pc_host/src/pc_vulkan_wall_lit_stream.c
  new/pc_host/src/pc_vulkan_wall_lighting.cpp
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/pc_host/src/pc_vulkan_wall_oracle_capture.c
  new/pc_host/src/pc_vulkan_wall_pixel_oracle.c
  new/jaguar/avp_gpu/world_texture_transfer.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/maze2_column.c
  new/jaguar/avp_gpu/projection.cpp
  new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_wall_mixed_fixture_r348 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_mixed_fixture_r348 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_mixed_fixture_r348 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_pc_vulkan_wall_mixed_fixture_r348 PRIVATE
    -Wl,--gc-sections)
endif()
add_test(NAME pc_vulkan_wall_mixed_fixture_r348
  COMMAND test_pc_vulkan_wall_mixed_fixture_r348)

# RE47 R353: SDK-enabled EXPERIMENTAL one-frame game wall Vulkan probe.
# This is not the gameplay Hardware renderer. OFF by default; the normal
# GCC/Clang executables and Software presentation do not link Vulkan.
option(AVP_BUILD_VULKAN_GAME_PROBE
    "Compile an opt-in live game ray/texture Vulkan GPU diagnostic" OFF)
if(AVP_BUILD_VULKAN_GAME_PROBE)
  if(NOT TARGET avp_pc OR NOT UNIX OR APPLE OR WIN32 OR
     NOT AVP_BUILD_VULKAN_WSI_DIAGNOSTIC OR
     NOT AVP_BUILD_VULKAN_SHADER_DIAGNOSTIC)
    message(FATAL_ERROR "Vulkan game probe requires Linux/X11 avp_pc, WSI and shader diagnostics")
  endif()
  target_sources(avp_pc PRIVATE
      new/pc_host/src/pc_hybrid_shader_paths.c
      new/pc_host/src/pc_hybrid_frame_gate.c
      new/pc_host/src/pc_vulkan_direct_rays.c
      new/pc_host/src/pc_vulkan_wsi.c
      new/pc_host/src/pc_hybrid_pipeline_cache.c
      new/pc_host/src/pc_hybrid_shader_cache.c
      new/pc_host/src/pc_hybrid_gpu_parity.c
      new/pc_host/src/pc_hybrid_stream_policy.c
      new/pc_host/src/pc_vulkan_hybrid_stream.c
      new/pc_host/src/pc_vulkan_wsi_policy.c
      new/pc_host/src/pc_vulkan_frame_policy.c
      new/pc_host/src/pc_vulkan_upload.c
      new/pc_host/src/pc_vulkan_upload_policy.c
      new/pc_host/src/pc_vulkan_descriptors.c
      new/pc_host/src/pc_vulkan_descriptor_policy.c
      new/pc_host/src/pc_vulkan_spirv_contract.c
      new/pc_host/src/pc_vulkan_opaque_pipeline.c
      new/pc_host/src/pc_vulkan_readback.c
      new/pc_host/src/pc_vulkan_wall_oracle_readback.c
      new/pc_host/src/pc_vulkan_wall_pixel_oracle.c
      new/pc_host/src/pc_vulkan_wall_oracle_capture.c
      new/pc_host/src/pc_vulkan_wall_mixed_fixture.c
      new/pc_host/src/pc_vulkan_wall_live_frame.c
      new/pc_host/src/pc_vulkan_flat_frame.c
      new/pc_host/src/pc_vulkan_flat_projection.c
      new/pc_host/src/pc_vulkan_layered_frame.c
      new/pc_host/src/pc_hybrid_overlay.c
      new/pc_host/src/pc_hybrid_world_gate.c
      new/pc_host/src/pc_hybrid_viewport.c
      new/pc_host/src/pc_hybrid_texture_cache.c
      new/pc_host/src/pc_vulkan_wall_layers.c
      new/pc_host/src/pc_vulkan_wall_lit_stream.c
      new/pc_host/src/pc_vulkan_wall_lighting.cpp
      new/pc_host/src/pc_vulkan_readback_policy.c
      new/pc_host/src/pc_vulkan_geometry_upload.c
      new/pc_host/src/pc_vulkan_geometry_policy.c
      new/pc_host/src/pc_vulkan_ray_columns.c
      new/pc_host/src/pc_vulkan_wall_projection.c
      new/pc_host/src/pc_vulkan_wall_slots.c
      new/pc_host/src/pc_vulkan_wall_source_upload.c
      new/pc_host/src/pc_vulkan_wall_image_bank.c
      new/pc_host/src/pc_vulkan_wall_draw_batch.c
      new/pc_host/src/pc_vulkan_wall_gpu_bank.c)
  target_link_libraries(avp_pc PRIVATE Vulkan::Vulkan)
  set_target_properties(avp_pc PROPERTIES SKIP_BUILD_RPATH TRUE)
  target_compile_definitions(avp_pc PRIVATE
      AVP_VK_GAMEPLAY_WALL_PROBE_BUILD=1
      "AVP_VK_GAME_PROBE_SPV_DIR=\"${AVP_SHADER_OUTPUT}\"")
  add_dependencies(avp_pc avp_vk_shaders_r327
      avp_vk_two_slot_wall_shader_r337 avp_vk_flat_shader_r355
      avp_vk_hybrid_foreground_shader_r360
      avp_vk_hybrid_present_shader_r361)
endif()

# RE47 R355: SDK-independent exact 3D-world floor/ceiling texel references.
# The rendering path is untouched; C++ retail projection shade is authoritative.
add_executable(test_pc_vulkan_flat_projection_r355
  tests/test_pc_vulkan_flat_projection_r355.cpp
  new/pc_host/src/pc_vulkan_flat_projection.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/projection.cpp)
target_include_directories(test_pc_vulkan_flat_projection_r355 PRIVATE include)
target_link_libraries(test_pc_vulkan_flat_projection_r355 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_flat_projection_r355 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_pc_vulkan_flat_projection_r355 PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME pc_vulkan_flat_projection_r355
  COMMAND test_pc_vulkan_flat_projection_r355
    "${CMAKE_CURRENT_SOURCE_DIR}/new/pc_host/src/shaders/avp_diag_flat.frag")

# RE47/R356: SDK-neutral floors/ceilings + walls source-backed composition.
add_executable(test_pc_vulkan_flat_frame_r356
  tests/test_pc_vulkan_flat_frame_r356.cpp
  new/pc_host/src/pc_vulkan_flat_frame.c
  new/pc_host/src/pc_vulkan_flat_projection.c
  new/pc_host/src/pc_vulkan_wall_oracle_capture.c
  new/pc_host/src/pc_vulkan_wall_pixel_oracle.c
  new/pc_host/src/pc_vulkan_wall_slots.c
  new/pc_host/src/pc_vulkan_wall_projection.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/world_texture_transfer.c
  new/jaguar/avp_gpu/projection.cpp
  new/jaguar/avp_gpu/maze2_column.c
  new/jaguar/hw/cry.c)
target_include_directories(test_pc_vulkan_flat_frame_r356 PRIVATE include)
target_link_libraries(test_pc_vulkan_flat_frame_r356 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_flat_frame_r356 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_pc_vulkan_flat_frame_r356 PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME pc_vulkan_flat_frame_r356 COMMAND test_pc_vulkan_flat_frame_r356)

# RE47/R357: layered MAZE2 wall/door/window source geometry.
# No GPU, SDK, ROM or private artwork required for the strict parity tests.
add_executable(test_pc_vulkan_wall_layers_r357
  tests/test_pc_vulkan_wall_layers_r357.cpp
  new/pc_host/src/pc_vulkan_wall_layers.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/maze2_column.c)
target_include_directories(test_pc_vulkan_wall_layers_r357 PRIVATE include)
target_link_libraries(test_pc_vulkan_wall_layers_r357 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_wall_layers_r357 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_pc_vulkan_wall_layers_r357 PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME pc_vulkan_wall_layers_r357 COMMAND test_pc_vulkan_wall_layers_r357)

# RE47/R358: full original MAZE2 layered window/door visibility to AVFL GPU
# texel-reference stream. No Vulkan SDK or active-game modification required.
add_executable(test_pc_vulkan_layered_frame_r358
  tests/test_pc_vulkan_layered_frame_r358.cpp
  new/pc_host/src/pc_vulkan_layered_frame.c
  new/pc_host/src/pc_vulkan_flat_projection.c
  new/pc_host/src/pc_vulkan_wall_layers.c
  new/jaguar/avp_gpu/world_scene_packet.c
  new/jaguar/avp_gpu/world_texture_transfer.c
  new/jaguar/avp_gpu/maze2_column.c
  new/jaguar/hw/cry.c
  new/jaguar/avp_gpu/projection.cpp)
target_include_directories(test_pc_vulkan_layered_frame_r358 PRIVATE include)
target_link_libraries(test_pc_vulkan_layered_frame_r358 PRIVATE m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_vulkan_layered_frame_r358 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_pc_vulkan_layered_frame_r358 PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME pc_vulkan_layered_frame_r358 COMMAND test_pc_vulkan_layered_frame_r358)

# RE47 R360: SDK-independent exact Software foreground extraction for a real
# Vulkan world + actor/weapon/HUD composition. No enabled GPU presentation.
add_executable(test_pc_hybrid_overlay_r360
  tests/test_pc_hybrid_overlay_r360.c
  new/pc_host/src/pc_hybrid_overlay.c)
target_include_directories(test_pc_hybrid_overlay_r360 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_overlay_r360 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_overlay_r360 COMMAND test_pc_hybrid_overlay_r360)

# RE47 R361: SDK-independent complete-source-world versus Software-world
# eligibility gate. A mismatch means the entire frame stays Software.
add_executable(test_pc_hybrid_world_gate_r361
  tests/test_pc_hybrid_world_gate_r361.c
  new/pc_host/src/pc_hybrid_world_gate.c)
target_include_directories(test_pc_hybrid_world_gate_r361 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_world_gate_r361 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME pc_hybrid_world_gate_r361 COMMAND test_pc_hybrid_world_gate_r361)

# RE47 R361: X11 letterbox and nearest-neighbor mapping shared with the
# experimental Vulkan game-window render path.
add_executable(test_pc_hybrid_viewport_r361
  tests/test_pc_hybrid_viewport_r361.c
  new/pc_host/src/pc_hybrid_viewport.c)
target_include_directories(test_pc_hybrid_viewport_r361 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_viewport_r361 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME pc_hybrid_viewport_r361 COMMAND test_pc_hybrid_viewport_r361)

# RE47 R362: exact-source GPU texture retention/invalidations without SDK.
# Prevents stale images on doors/level changes/hi-res replacements.
add_executable(test_pc_hybrid_texture_cache_r362
  tests/test_pc_hybrid_texture_cache_r362.c
  new/pc_host/src/pc_hybrid_texture_cache.c)
target_include_directories(test_pc_hybrid_texture_cache_r362 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_texture_cache_r362 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_texture_cache_r362 COMMAND test_pc_hybrid_texture_cache_r362)

# RE47 R363: SDK-independent shader/region pipeline-cache correctness gate.
add_executable(test_pc_hybrid_pipeline_cache_r363
  tests/test_pc_hybrid_pipeline_cache_r363.c
  new/pc_host/src/pc_hybrid_pipeline_cache.c)
target_include_directories(test_pc_hybrid_pipeline_cache_r363 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_pipeline_cache_r363 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_pipeline_cache_r363 COMMAND test_pc_hybrid_pipeline_cache_r363)

# R364: SDK-independent growth and frame-size policy gates.
add_executable(test_pc_hybrid_stream_policy_r364
  tests/test_pc_hybrid_stream_policy_r364.c
  new/pc_host/src/pc_hybrid_stream_policy.c)
target_include_directories(test_pc_hybrid_stream_policy_r364 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_stream_policy_r364 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_stream_policy_r364 COMMAND test_pc_hybrid_stream_policy_r364)

# RE48 R367: fail-closed, exact-content foreground GPU transfer reuse.
add_executable(test_pc_hybrid_foreground_reuse_r367
  tests/test_pc_hybrid_foreground_reuse_r367.c
  new/pc_host/src/pc_hybrid_stream_policy.c)
target_include_directories(test_pc_hybrid_foreground_reuse_r367 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_foreground_reuse_r367 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_foreground_reuse_r367
  COMMAND test_pc_hybrid_foreground_reuse_r367)

# RE47 R365: SDK-independent three-shader transactional cache preflight.
add_executable(test_pc_hybrid_shader_cache_r365
  tests/test_pc_hybrid_shader_cache_r365.c
  new/pc_host/src/pc_hybrid_shader_cache.c
  new/pc_host/src/pc_vulkan_spirv_contract.c)
target_include_directories(test_pc_hybrid_shader_cache_r365 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_shader_cache_r365 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_shader_cache_r365
  COMMAND test_pc_hybrid_shader_cache_r365)

# R365: full GPU output versus original software pixel proof, no Vulkan SDK.
add_executable(test_pc_hybrid_gpu_parity_r365
  tests/test_pc_hybrid_gpu_parity_r365.c
  new/pc_host/src/pc_hybrid_gpu_parity.c
  new/pc_host/src/pc_hybrid_viewport.c
  new/pc_host/src/pc_vulkan_readback_policy.c)
target_include_directories(test_pc_hybrid_gpu_parity_r365 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_gpu_parity_r365 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_gpu_parity_r365 COMMAND test_pc_hybrid_gpu_parity_r365)

# R368 SDK-independent relocatable experimental Vulkan shader-pack contract.
add_executable(test_pc_hybrid_shader_paths_r368
    tests/test_pc_hybrid_shader_paths_r368.c
    new/pc_host/src/pc_hybrid_shader_paths.c)
target_include_directories(test_pc_hybrid_shader_paths_r368 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_shader_paths_r368 PRIVATE
      -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_shader_paths_r368 COMMAND test_pc_hybrid_shader_paths_r368)

# R372: no expensive Hybrid scene generation for known software-only frames.
add_executable(test_pc_hybrid_frame_gate_r372
  tests/test_pc_hybrid_frame_gate_r372.c
  new/pc_host/src/pc_hybrid_frame_gate.c)
target_include_directories(test_pc_hybrid_frame_gate_r372 PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_frame_gate_r372 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hybrid_frame_gate_r372 COMMAND test_pc_hybrid_frame_gate_r372)

# R374: crosshair/reticle pixel parity, SDK independent.
add_executable(test_pc_hybrid_reticle_r374 tests/test_pc_hybrid_reticle_r374.c)
target_include_directories(test_pc_hybrid_reticle_r374 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hybrid_reticle_r374 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME pc_hybrid_reticle_r374 COMMAND test_pc_hybrid_reticle_r374)

# R378: mock the GPU ownership boundary for deferred per-slot uploads.
# Unused standalone synchronous-build functions are excluded by section GC.
if(AVP_BUILD_VULKAN_WSI_DIAGNOSTIC AND BUILD_TESTING)
  add_executable(test_pc_vulkan_wall_gpu_adopt_r378
    tests/test_pc_vulkan_wall_gpu_adopt_r378.c
    new/pc_host/src/pc_vulkan_wall_gpu_bank.c
    new/pc_host/src/pc_vulkan_wall_image_bank.c)
  target_include_directories(test_pc_vulkan_wall_gpu_adopt_r378 PRIVATE include ${Vulkan_INCLUDE_DIRS})
  target_compile_options(test_pc_vulkan_wall_gpu_adopt_r378 PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_pc_vulkan_wall_gpu_adopt_r378 PRIVATE -Wl,--gc-sections)
  add_test(NAME pc_vulkan_wall_gpu_adopt_r378 COMMAND test_pc_vulkan_wall_gpu_adopt_r378)
endif()

# R384: actual Software FPS and local-DM X11 child-overlay implementation.
# Full game symbols not involved in overlay painting are discarded by section
# GC. The test exercises XShape input pass-through under a live virtual server,
# including parent-frame repaint, F12 gate, and menu hide transitions.
if(UNIX AND NOT APPLE AND AVP_BUILD_PC AND BUILD_TESTING)
  add_executable(test_r384_x11_host_overlay tests/test_r384_x11_host_overlay.c)
  target_include_directories(test_r384_x11_host_overlay PRIVATE
    include new/map_maker new/game_micro_optimization new/multiplayer third_party/musashi)
  target_compile_definitions(test_r384_x11_host_overlay PRIVATE
    AVP_LOCAL_MULTIPLAYER=1 AVP_GAME_MICRO_OPT_WRAP=1
    AVP_PC_NATIVE_TITLE_JAGM=1 AVP_PC_REFERENCE_TITLE_TIMING=1)
  target_compile_options(test_r384_x11_host_overlay PRIVATE
    -ffunction-sections -fdata-sections -UNDEBUG)
  target_link_options(test_r384_x11_host_overlay PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_r384_x11_host_overlay PRIVATE
    ${X11_LIBRARIES} ${CMAKE_DL_LIBS} m Threads::Threads)
  find_program(AVP_R384_XVFB_RUN xvfb-run)
  if(AVP_R384_XVFB_RUN)
    add_test(NAME r384_x11_host_overlay
      COMMAND ${AVP_R384_XVFB_RUN} -a -s "-screen 0 1024x768x24"
              $<TARGET_FILE:test_r384_x11_host_overlay>)
  endif()
endif()

# R385: full-scan original vs native-row-bounded host HUD replay at several
# target sizes, foreground sparsity patterns and background masks.
add_executable(test_pc_hud_bounds_r385
  tests/test_pc_hud_bounds_r385.c new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_hud_bounds_r385 PRIVATE include)
target_link_libraries(test_pc_hud_bounds_r385 PRIVATE avp_jaguar_compat m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_hud_bounds_r385 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_hud_bounds_r385 COMMAND test_pc_hud_bounds_r385)

# R386: source-identity lookup and sparse flat preheat regressions.
add_executable(test_pc_texture_index_r386 tests/test_pc_texture_index_r386.c
  new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_pc_texture_index_r386 PRIVATE include)
target_link_libraries(test_pc_texture_index_r386 PRIVATE avp_jaguar_compat m)
add_executable(test_pc_sparse_flats_r386 tests/test_pc_sparse_flats_r386.c)
target_include_directories(test_pc_sparse_flats_r386 PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_pc_texture_index_r386 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_compile_options(test_pc_sparse_flats_r386 PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME pc_texture_index_r386 COMMAND test_pc_texture_index_r386)
add_test(NAME pc_sparse_flats_r386 COMMAND test_pc_sparse_flats_r386)

# R387: independently test the XShm frame-buffer ownership protocol.
# The test includes the actual private helper implementation and never mocks
# X server completion events; requires a live MIT-SHM-capable Xvfb server.
if(UNIX AND NOT APPLE AND AVP_BUILD_PC AND BUILD_TESTING)
  if(X11_XShm_FOUND AND X11_Xext_LIB)
    set(AVP_R387_XEXT_LIB "${X11_Xext_LIB}")
    set(AVP_R387_XSHM_COMPAT 0)
  elseif(IS_ABSOLUTE "${AVP_XEXT_SONAME_PATH}" AND EXISTS "${AVP_XEXT_SONAME_PATH}")
    set(AVP_R387_XEXT_LIB "${AVP_XEXT_SONAME_PATH}")
    set(AVP_R387_XSHM_COMPAT 1)
  endif()
  if(AVP_R387_XEXT_LIB)
    add_executable(test_r387_xshm_double_buffer tests/test_r387_xshm_double_buffer.c)
    target_include_directories(test_r387_xshm_double_buffer PRIVATE
      include new/map_maker new/game_micro_optimization new/multiplayer third_party/musashi)
    target_compile_definitions(test_r387_xshm_double_buffer PRIVATE
      AVP_HAVE_XSHM=1 AVP_LOCAL_MULTIPLAYER=1 AVP_GAME_MICRO_OPT_WRAP=1
      AVP_PC_NATIVE_TITLE_JAGM=1 AVP_PC_REFERENCE_TITLE_TIMING=1)
    if(AVP_R387_XSHM_COMPAT)
      target_compile_definitions(test_r387_xshm_double_buffer PRIVATE AVP_XSHM_COMPAT_DECLS=1)
    endif()
    target_compile_options(test_r387_xshm_double_buffer PRIVATE
      -Wall -Wextra -Werror -ffunction-sections -fdata-sections -UNDEBUG)
    target_link_options(test_r387_xshm_double_buffer PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_r387_xshm_double_buffer PRIVATE
      ${X11_LIBRARIES} ${AVP_R387_XEXT_LIB} ${CMAKE_DL_LIBS} m Threads::Threads)
    find_program(AVP_R387_XVFB_RUN xvfb-run)
    if(AVP_R387_XVFB_RUN)
      add_test(NAME r387_xshm_double_buffer
        COMMAND ${AVP_R387_XVFB_RUN} -a -s "-screen 0 1024x768x24"
                $<TARGET_FILE:test_r387_xshm_double_buffer>)
    endif()
  endif()
endif()

# R388: worker-local filtered wall shade LUT reuse, byte-for-byte oracle.
if(NOT WIN32)
  add_executable(test_r388_wall_shade_cache tests/test_r388_wall_shade_cache.c)
  target_include_directories(test_r388_wall_shade_cache PRIVATE include)
  target_link_libraries(test_r388_wall_shade_cache PRIVATE pthread)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_r388_wall_shade_cache PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME r388_wall_shade_cache COMMAND test_r388_wall_shade_cache)
endif()

add_executable(test_r388_fp_column_uv tests/test_r388_fp_column_uv.c)
target_include_directories(test_r388_fp_column_uv PRIVATE include)
if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_r388_fp_column_uv PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME r388_fp_column_uv COMMAND test_r388_fp_column_uv)

# R389: source-fidelity exact-output floor/ceiling and AAF scratch regressions.
# Checks remain enabled even in Release builds and run without a display/ROM.
foreach(r389_test test_r389_floor_cell_and_shade test_r389_sprite_scratch)
  add_executable(${r389_test} tests/${r389_test}.c)
  target_include_directories(${r389_test} PRIVATE include)
  if(AVP_STRICT AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${r389_test} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME ${r389_test} COMMAND ${r389_test})
endforeach()

# R390: original-versus-reused AAF mip chain, 320 frame/original pixels pairs.
add_executable(test_r390_sprite_mip_reuse tests/test_r390_sprite_mip_reuse.c
  new/jaguar/avp_gpu/texture_filter.c)
target_include_directories(test_r390_sprite_mip_reuse PRIVATE include)
target_link_libraries(test_r390_sprite_mip_reuse PRIVATE avp_jaguar_compat m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_r390_sprite_mip_reuse PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME r390_sprite_mip_reuse COMMAND test_r390_sprite_mip_reuse)

# R390: exact full-frame postprocessing plus filtered actor row coordinates.
add_executable(test_r390_vignette_sparse tests/test_r390_vignette_sparse.c
  new/pc_host/src/pc_vignette.c)
target_include_directories(test_r390_vignette_sparse PRIVATE include)
add_executable(test_r390_sprite_row_uv tests/test_r390_sprite_row_uv.c)
target_include_directories(test_r390_sprite_row_uv PRIVATE include)
foreach(r390_test test_r390_vignette_sparse test_r390_sprite_row_uv)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${r390_test} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME ${r390_test} COMMAND ${r390_test})
endforeach()

# RE49 R391: new PC-only health/pause policy checks.
add_executable(test_r391_health new/cheats/tests/test_r391_health.c)
add_executable(test_r391_pause new/pc_video/tests/test_r391_pause.c
    new/jaguar/hw/object_processor.c new/jaguar/hw/cry.c new/jaguar/hw/framebuffer.c)
target_include_directories(test_r391_pause PRIVATE include)
target_link_libraries(test_r391_pause PRIVATE m)
foreach(t test_r391_health test_r391_pause)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${t} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  endif()
  add_test(NAME ${t} COMMAND ${t})
endforeach()

# RE49 R392: reject unreviewed changes to protected gameplay source.
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME r392_protected_game_source
    COMMAND ${Python3_EXECUTABLE} ${AVP_ORIGINAL_SOURCE_ROOT}/new/source_integrity/check_game_source.py)
endif()

# R398: PC-only interpolated damage-screen color.
add_executable(test_r398_damage_presentation new/pc_video/tests/test_r398_damage_presentation.c)
target_include_directories(test_r398_damage_presentation PRIVATE include)
target_link_libraries(test_r398_damage_presentation PRIVATE avp_jaguar_compat)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_r398_damage_presentation PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME test_r398_damage_presentation COMMAND test_r398_damage_presentation)

# R399 PC-only allocator and mixer integration, synthetic samples only.
if(UNIX)
  add_executable(test_r399_sound_channels new/audio/tests/test_sound_channels.c
    src/game/music.c src/game/runtime.c new/jaguar/host/jerry_wave_procedural.c)
  target_include_directories(test_r399_sound_channels PRIVATE include new/multiplayer)
  target_compile_definitions(test_r399_sound_channels PRIVATE AVP_PC_HOST=1 AVP_LOCAL_MULTIPLAYER=1)
  target_compile_options(test_r399_sound_channels PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(test_r399_sound_channels PRIVATE m pthread)
  add_test(NAME test_r399_sound_channels COMMAND test_r399_sound_channels)
endif()

# R400: actual native AMP damage boundary, with the PC-only gate active.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_r400_native_damage new/cheats/tests/test_r400_native_damage.c
    new/cheats/damage_gate.c src/game/runtime.c)
  target_include_directories(test_r400_native_damage PRIVATE include)
  target_compile_definitions(test_r400_native_damage PRIVATE AVP_NEW_INVINCIBILITY_GATE=1)
  target_compile_options(test_r400_native_damage PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
  target_link_options(test_r400_native_damage PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_r400_native_damage PRIVATE m)
  add_test(NAME r400_native_damage COMMAND test_r400_native_damage)
endif()

# R401: integer scanline output and Hybrid Vulkan pixel proof.
add_executable(test_r401_scanlines new/pc_host/tests/test_r401_scanlines.c
  new/pc_host/src/pc_hybrid_gpu_parity.c new/pc_host/src/pc_hybrid_viewport.c
  new/pc_host/src/pc_vulkan_readback_policy.c)
target_include_directories(test_r401_scanlines PRIVATE include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test_r401_scanlines PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME r401_scanlines COMMAND test_r401_scanlines)

# R403: test real original collision routines versus optional PC alternatives.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_game_micro_vector_moves
    new/game_micro_optimization/tests/test_vector_moves.c
    new/game_micro_optimization/game_micro_optimization.c
    src/game/collision.c)
  target_compile_definitions(test_game_micro_vector_moves PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_vector_moves PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_vector_moves PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_vector_moves PRIVATE -Wl,-gc-sections -Wl,--wrap=Vector -Wl,--wrap=NMoves -Wl,--wrap=avp_collision_bind_maze)
  add_test(NAME game_micro_vector_moves_r403 COMMAND test_game_micro_vector_moves)
endif()

# R404: exact straight-axis LOS against the original-derived scanner.
if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_game_micro_axis_los
    new/game_micro_optimization/tests/test_axis_los.c
    new/game_micro_optimization/game_micro_optimization.c src/game/collision.c)
  target_compile_definitions(test_game_micro_axis_los PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_axis_los PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_axis_los PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_axis_los PRIVATE -Wl,-gc-sections -Wl,--wrap=LineOfSight -Wl,--wrap=avp_collision_bind_maze)
  add_test(NAME game_micro_axis_los_r404 COMMAND test_game_micro_axis_los)
endif()

# R405: isolated comparison against the unchanged original-derived routines.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND UNIX AND NOT APPLE)
  foreach(kind oblique_los trig)
    if(kind STREQUAL "trig")
      set(ref src/game/maze.c)
      set(wraps -Wl,--wrap=sin_d0 -Wl,--wrap=cos_d0)
    else()
      set(ref src/game/collision.c)
      set(wraps -Wl,--wrap=LineOfSight)
    endif()
    add_executable(test_game_micro_${kind} new/game_micro_optimization/tests/test_${kind}.c new/game_micro_optimization/game_micro_optimization.c ${ref})
    target_compile_definitions(test_game_micro_${kind} PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
    target_include_directories(test_game_micro_${kind} PRIVATE include new/game_micro_optimization)
    target_compile_options(test_game_micro_${kind} PRIVATE -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
    target_link_options(test_game_micro_${kind} PRIVATE -Wl,-gc-sections ${wraps} -Wl,--wrap=avp_collision_bind_maze)
    add_test(NAME game_micro_${kind} COMMAND test_game_micro_${kind})
  endforeach()
endif()

# R406: occupied AMP collision parity and reproducible occupancy benchmarks.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND UNIX AND NOT APPLE)
  add_executable(test_game_micro_amp_occupied new/game_micro_optimization/tests/test_amp_occupied.c new/game_micro_optimization/game_micro_optimization.c src/game/collision.c)
  target_compile_definitions(test_game_micro_amp_occupied PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_amp_occupied PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_amp_occupied PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_amp_occupied PRIVATE -Wl,-gc-sections -Wl,--wrap=AMPCollisions -Wl,--wrap=avp_collision_bind_maze)
  add_test(NAME game_micro_amp_occupied_r406 COMMAND test_game_micro_amp_occupied)
endif()

# R407: single-edge SafePos and live collision rebinding parity.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND UNIX AND NOT APPLE)
  add_executable(test_game_micro_safe_pos_edge new/game_micro_optimization/tests/test_safe_pos_edge.c new/game_micro_optimization/game_micro_optimization.c src/game/collision.c)
  target_compile_definitions(test_game_micro_safe_pos_edge PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_safe_pos_edge PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_safe_pos_edge PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_safe_pos_edge PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=avp_collision_bind_maze)
  add_test(NAME game_micro_safe_pos_edge_r407 COMMAND test_game_micro_safe_pos_edge)
endif()

# R408: exact nonoverlapping SafePos corner path.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND UNIX AND NOT APPLE)
  add_executable(test_game_micro_safe_pos_corner new/game_micro_optimization/tests/test_safe_pos_corner.c new/game_micro_optimization/game_micro_optimization.c src/game/collision.c)
  target_compile_definitions(test_game_micro_safe_pos_corner PRIVATE AVP_GAME_MICRO_OPT_WRAP=1)
  target_include_directories(test_game_micro_safe_pos_corner PRIVATE include new/game_micro_optimization)
  target_compile_options(test_game_micro_safe_pos_corner PRIVATE -O3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
  target_link_options(test_game_micro_safe_pos_corner PRIVATE -Wl,-gc-sections -Wl,--wrap=SafePos -Wl,--wrap=avp_collision_bind_maze)
  add_test(NAME game_micro_safe_pos_corner_r408 COMMAND test_game_micro_safe_pos_corner)
endif()

# R437: source-linked retail actor collision slide and Queen approach gate.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_player_amp_slide_r437 new/gameplay_fidelity/tests/test_player_amp_slide_r437.c src/game/player.c src/game/collision.c)
  add_executable(test_queen_approach_r437 new/gameplay_fidelity/tests/test_queen_approach_r437.c tests/amp_link_stubs.c src/game/amp.c src/game/runtime.c)
  foreach(t test_player_amp_slide_r437 test_queen_approach_r437)
    target_include_directories(${t} PRIVATE include)
    target_compile_options(${t} PRIVATE -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -UNDEBUG)
    target_link_options(${t} PRIVATE -Wl,--gc-sections)
    target_link_libraries(${t} PRIVATE m)
    add_test(NAME ${t} COMMAND ${t})
  endforeach()
endif()

# R456: exercise real AMP save/rebuild code with deterministic authored placements.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_enemy_health_r456 new/gameplay_enhancements/tests/test_enemy_health_r456.c src/game/runtime.c tests/amp_link_stubs.c)
  target_include_directories(test_enemy_health_r456 PRIVATE include)
  target_compile_options(test_enemy_health_r456 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_enemy_health_r456 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_enemy_health_r456 PRIVATE m)
  add_test(NAME enemy_health_r456 COMMAND test_enemy_health_r456)
endif()

# R457: verify independent enemy/hazard persistence.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_hazard_health_r457 new/gameplay_enhancements/tests/test_hazard_health_r457.c src/game/runtime.c tests/amp_link_stubs.c)
  target_include_directories(test_hazard_health_r457 PRIVATE include)
  target_compile_options(test_hazard_health_r457 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_hazard_health_r457 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_hazard_health_r457 PRIVATE m)
  add_test(NAME hazard_health_r457 COMMAND test_hazard_health_r457)
endif()

# R458: real native generator death and barrel area-damage parity.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_generator_blast_r458 new/gameplay_enhancements/tests/test_generator_blast_r458.c src/game/runtime.c src/game/collision.c tests/amp_link_stubs.c new/cheats/damage_gate.c)
  target_include_directories(test_generator_blast_r458 PRIVATE include)
  target_compile_definitions(test_generator_blast_r458 PRIVATE AVP_TEST_REAL_COLLISION=1 AVP_NEW_INVINCIBILITY_GATE=1)
  target_compile_options(test_generator_blast_r458 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_generator_blast_r458 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_generator_blast_r458 PRIVATE m)
  add_test(NAME generator_blast_r458 COMMAND test_generator_blast_r458)
endif()

# R459: PC-only enemy population modifier, expanded pool and revisit semantics.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_enemy_multiplier_r459 new/gameplay_modifiers/tests/test_enemy_multiplier_r459.c src/game/runtime.c src/game/collision.c tests/amp_link_stubs.c)
  target_include_directories(test_enemy_multiplier_r459 PRIVATE include)
  target_compile_definitions(test_enemy_multiplier_r459 PRIVATE AVP_TEST_REAL_COLLISION=1)
  target_compile_options(test_enemy_multiplier_r459 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_enemy_multiplier_r459 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_enemy_multiplier_r459 PRIVATE m)
  add_test(NAME enemy_multiplier_r459 COMMAND test_enemy_multiplier_r459)
  add_executable(test_vent_facehuggers_r463 new/gameplay_modifiers/tests/test_vent_facehuggers_r463.c src/game/runtime.c src/game/collision.c tests/amp_link_stubs.c)
  target_include_directories(test_vent_facehuggers_r463 PRIVATE include)
  target_compile_definitions(test_vent_facehuggers_r463 PRIVATE AVP_TEST_REAL_COLLISION=1)
  target_compile_options(test_vent_facehuggers_r463 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_vent_facehuggers_r463 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_vent_facehuggers_r463 PRIVATE m)
  add_test(NAME vent_facehuggers_r463 COMMAND test_vent_facehuggers_r463)
endif()

# R460: committed crawler attack, optional immediate death interruption.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_crawl_death_interrupt_r460 new/gameplay_enhancements/tests/test_crawl_death_interrupt_r460.c src/game/runtime.c src/game/collision.c tests/amp_link_stubs.c new/cheats/damage_gate.c)
  target_include_directories(test_crawl_death_interrupt_r460 PRIVATE include)
  target_compile_definitions(test_crawl_death_interrupt_r460 PRIVATE AVP_TEST_REAL_COLLISION=1 AVP_NEW_INVINCIBILITY_GATE=1)
  target_compile_options(test_crawl_death_interrupt_r460 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_crawl_death_interrupt_r460 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_crawl_death_interrupt_r460 PRIVATE m)
  add_test(NAME crawl_death_interrupt_r460 COMMAND test_crawl_death_interrupt_r460)
endif()

# R462: isolated NPC stat modifiers, native attack paths and attached facehug damage.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_enemy_stats_r462 new/gameplay_modifiers/tests/test_enemy_stats_r462.c src/game/runtime.c src/game/collision.c tests/amp_link_stubs.c)
  target_include_directories(test_enemy_stats_r462 PRIVATE include)
  target_compile_definitions(test_enemy_stats_r462 PRIVATE AVP_TEST_REAL_COLLISION=1)
  target_compile_options(test_enemy_stats_r462 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_enemy_stats_r462 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_enemy_stats_r462 PRIVATE m)
  add_test(NAME enemy_stats_r462 COMMAND test_enemy_stats_r462)
  add_executable(test_facehugger_damage_r462 new/gameplay_modifiers/tests/test_facehugger_damage_r462.c src/game/mazescrn.c src/game/runtime.c)
  target_include_directories(test_facehugger_damage_r462 PRIVATE include)
  target_compile_options(test_facehugger_damage_r462 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_facehugger_damage_r462 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_facehugger_damage_r462 PRIVATE m)
  add_test(NAME facehugger_damage_r462 COMMAND test_facehugger_damage_r462)
endif()

# R464: full-travel host analog velocity against actual MovePlayer.
if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_analog_velocity_r464 new/pc_video/tests/test_analog_velocity_r464.c src/game/player.c)
  target_include_directories(test_analog_velocity_r464 PRIVATE include)
  target_compile_options(test_analog_velocity_r464 PRIVATE -O2 -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_analog_velocity_r464 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_analog_velocity_r464 PRIVATE m)
  add_test(NAME analog_velocity_r464 COMMAND test_analog_velocity_r464)
endif()

if(UNIX AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(test_facehugger_instant_death_r469 new/gameplay_modifiers/tests/test_facehugger_instant_death_r469.c src/game/runtime.c src/game/collision.c tests/amp_link_stubs.c new/cheats/damage_gate.c)
  target_include_directories(test_facehugger_instant_death_r469 PRIVATE include)
  target_compile_definitions(test_facehugger_instant_death_r469 PRIVATE AVP_TEST_REAL_COLLISION=1 AVP_NEW_INVINCIBILITY_GATE=1)
  target_compile_options(test_facehugger_instant_death_r469 PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(test_facehugger_instant_death_r469 PRIVATE -Wl,--gc-sections)
  target_link_libraries(test_facehugger_instant_death_r469 PRIVATE m)
  add_test(NAME facehugger_instant_death_r469 COMMAND test_facehugger_instant_death_r469)
endif()
