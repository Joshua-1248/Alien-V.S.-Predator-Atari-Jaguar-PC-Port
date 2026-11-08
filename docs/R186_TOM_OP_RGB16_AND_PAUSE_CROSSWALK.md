# R186 — TOM Object Processor versus PC full-sweep audit

## ROM origin and verified data identity

Original `AVP.jag`: 4,194,304 bytes, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Executable stream at `$3804`: 99,072 bytes, SHA-256 `c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53`. Authored data at `$E840`: 60,248 bytes, SHA-256 `772745e9d429bce8efe884bc66461e754372e9cc22563dd50bd1268ab29bb8ba`. These prove the exact ROM/source binary sections, **not** full 68000/gameplay equivalence.

The ROM's FILES JagPEG header (version 1 with 8-pixel block columns/rows) gives these objective, directly extracted dimensions:

| FILES item | Payload | Decoded raster | MJP binding |
| --- | --- | --- | --- |
| 205 | JP80 | 96×40 | TITLE role 5 |
| 207 | JP80 | 320×80 | TITLE role 1 |
| 208 | JP80 | 320×280 | TITLE role 0 |

Original recovered `src/game/mjp.c::Make_TLi()` builds two **320-pixel logical width** scaled objects with source `Ob_Height` values 110 and 82. `Update()` modifies object slot 2's scale, `Update2()` modifies slot 4's. The host's present decoded-image-to-screen mapping remains approximated; these dimensions alone do not identify the exact buffer/overlay addresses for both title object sources. Do not extrapolate a direct image-sampling algorithm merely from the compression headers.

## Source-backed pixel fixes

`src/jaguar/avp_gpu/frontend_renderer.c`: the COLOR source of the title SCBITOBJ and the character carousel is RGB16 in the Object Processor's output path. Previously the transparency test used 16-bit packed zero while the visible title source was written as the decoder's 24-bit original (`0xf02819` rather than `0xf02818` in the selected synthetic fixture); the character-selection SCBITOBJ displayed arbitrary 8-bit component low bits but the SGCF cross handler already quantized to RGB16. Both now use the same RGB16 packing and unpacking helper, with unmodified object ordering, sample indices and phase state.

## Source-backed OP lifecycle fix

`src/game/mazescrn.c::SetMazeList()` resets CPU `in_pause` and sends `SML0` before installing a new maze display list. `RestoreMazeList()` sends `RML0` when replacing the current list. Hosted `src/jaguar/hw/object_processor.c` already handled pause PSVO/PRVO/PLNK but ignored SML0/RML0; consequently a previous pause branch and saved frame could remain attached to the new list. R186 resets the pause OP's active/menu/cursor/saved-picture state at these two list-reset events, preserving decoded artwork/palette ownership and the actual `DoPause` input logic.

## Verification limitations / next work

- The OP's full Jaguar phrase parsing, background/framebuffer selection, title picture memory layout and per-line scaling remain unverified; do not label the limited RGB16 corrections as full pixel-parity reconstruction.
- Hardware-vs-PC direct recordings are needed for title expansion and full character-selection GPU CROSS geometry, pause and computer final composition, and frontend transition timing.
- Existing JERRY wave/synthesis exactness and hard doorway AI geometry remain separately open; no speculative DSP/collision modifications in this pass.
- Original-derived `src/game` bytes are unchanged; all 33 files match R185.
- GCC/Clang strict release, both broad regression subsets, both test-family expected failures versus R185, UBSan and ROM identity verification passed. The original ROM Xvfb smoke initialized frontend resources but did not exercise a 3D game.

---
# Alien vs Predator (1994 Atari Jaguar) — PC port R186

**Baseline:** R185 (RE #19). **Scope:** Multi-path TOM Object Processor pixel and pause-list lifecycle sweep.

## Three targeted fixes

1. **Title scaled Object Processor sprites use RGB16 output precision.** The original Jaguar 16-bit OP line buffer does not retain the low RGB888 bits from the PC's JagPEG intermediate. `frontend_blit_title_sprite` previously tested RGB16 transparency but wrote unquantized decoded 24-bit colors in the fully visible sprite. It now packs and unpacks the same RGB16 pixel as its transparency/OP description. The original title asset choice, dimensions, motion and host-only reveal curve remain unchanged. The `reveal<16` branch is left as it was because its precise GPU blend hasn't been reconstructed; it is not used by the existing title call sites.

2. **Character carousel scaled OP objects use RGB16 output precision.** The SCBITOBJ presenter previously stored host RGB888 source texels in the final framebuffer, while the adjacent SGCF GPU CROSS handler already quantized to RGB16. It now stores the same RGB16-quantized value throughout the regular carousel. Original 3.5 fixed-point sampling, pose/scale tracks, cropping, timing and original character selection/gameplay are unchanged.

3. **Fresh MAZESCRN display lists clear retained pause/SaveOvers state.** Original `SetMazeList()` (`SML0`) and `RestoreMazeList()` (`RML0`) replace the OP refresh list and reset the source CPU pause state. The host `avp_jaguar_pause_op_event` previously ignored those list events, potentially retaining an active PAUSED overlay, cursor or old frame snapshot after the display-list handoff. It now disables the old pause branch and releases its stale saved picture while preserving all ROM-loaded pause brushes and palettes.

## Deliberately deferred after direct ROM inspection

The actual ROM's JagPEG compressed dimensions are FILES 205: **96×40**, FILES 207: **320×80**, FILES 208: **320×280**. The recovered `MJP::Make_TLi` builds *logical* scaled/overlay OP objects of widths 320 and heights 110/82, and `Update`/`Update2` update their scale phrases. Those authoring dimensions do not simply match all stored image dimensions. Replacing the present host title-geometry sampling with a trivial `floor(n*32/scale)` lookup directly into the compressed images **would be unsupported** until the original GPU/JagPEG destination buffer and OP memory phrase identities are reconstructed. No such guessed geometry replacement is in R186.

The Atari Jaguar SCBITOBJ uses 3.5 fixed-point HSCALE/VSCALE and TRANSPARENT/DEPTH semantics. We did not claim a cycle-accurate GPU/OP implementation or exact visual parity based only on decoded resources.

## Verification

- GCC and Clang strict Release builds: PASS.
- GCC and Clang full broad regression runners excluding the same three historically expensive tests (`test_amp_passage_geometry`, `test_amp_projectile`, `test_sprite_column_cache`): PASS.
- GCC and Clang focused new OP/front-end raster regressions: PASS.
- Both revised tests against unchanged R185 production code: expected FAIL (RGB16 output and stale pause state, respectively).
- GCC and Clang focused OP/frontend UBSan: PASS.
- Original ROM and decompressed RDB.TEXT/RDB.DTA hashes: MATCH.
- GCC and Clang original-ROM/Xvfb frontend startup: initialized resources and frontend successfully, then externally terminated at ten seconds; **not a gameplay/OP visual parity test**.
- All 33 original `src/game` files: byte-for-byte unchanged from R185.
- No full playable campaign runs, Jaguar hardware capture comparison, title image-buffer reconstruction, or exhaustive GPU/DSP verification performed.

## Rights and distribution

Distribute complete current C/C++ source, dual x86-64 Linux builds (mode 0755), credits/licenses, the historical R001–R186 addendum, preserved research and current verification. No commercial ROM, extracted proprietary assets, third-party wave ROM, private Ghidra database, prior binary release ZIPs, or user save files.
