# R187 — Original Jaguar TOM title-background aperture crosswalk

## Retail source versus host

1. **ROM resource:** `AVP.jag` (verified canonical 4 MiB, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`); tagged FILES 208 is `JP80` and directly decodes to 320×280.
2. **Retail 68000/object phrase:** `src/game/mjp.c::Make_TLi()` reconstructs the title base at `Ob_Pos_Y=Zero_Line, Ob_Width=320, Ob_Height=0x118 (280), Ob_Trans=0`, with an unscaled Object Processor phrase. The PC's `InitMJP()` makes the 320×240 display, with the Y origin at zero. The displayed raster therefore begins with source row zero and is clipped by the display height.
3. **R186 PC deviation:** `src/jaguar/avp_gpu/frontend_renderer.c::frontend_blit_image()` selected logical height 240 but initialized `crop_y=(280-240)/2=20`. This was generic host centering not present in the recovered TITLE bitmap object's geometry.
4. **R187 correction:** Only TITLE and the CREDITS title-background reuse are explicitly rendered with top-origin clipping when the source is 320×280. All other frontend mode images and animations remain unchanged.
5. **Original gameplay data:** All original-derived `src/game` files are byte-identical to R186. No source-authored sprite pose, pause state, frame timing, menus, audio, level transitions, or gameplay rules changed.

## Independent video comparison

The settled-title frame from the user's October 21 BigPEmu video shows the title's background art behind the logo and independent sprites. Extracting FILES 208 and comparing multiple source Y offsets in unaffected picture regions favored offset **0** (as opposed to the old offset 20); capture scaling/color decoding and independent objects prevent claiming an exact pixel-by-pixel certification.

## Additional audited paths deliberately left unchanged

| Subsystem | Finding | Why not modified |
| --- | --- | --- |
| Title FILES 205 and 207 | Decoded 96×40 and 320×80; logical MJP objects are 320-wide with 110/82-height phrases | Original decompress-to-object destination memory and per-line SCBITOBJ sampling still need recovery |
| Title Update/Update2 | CPU integer phase formulas present | No ROM-proven replacement for existing GPU scaled-picture mapping |
| Character selection SGCF | First-frame OP handoff is corrected in R174; subsequent 50-tick approach uses host interpolation | Requires original GPU CROSS microcode reconstruction |
| Hall of Fame | Existing fade and 3-native-pixel/VBlank score animation retained | Already measured from user footage; no conflicting original instruction trace found |
| CREDITS | Background aperture corrected in R187; host text scrolling remains approximate | Do not invent original credits OP control microcode |
| Pause and terminal OP | R186 SML0/RML0 pause-list cleanup remains | Need original line buffer/object phrase comparison for full composition |
| JERRY / FullSynth | Existing host synthesizer not modified | Original DSP trace and waveform tables remain incomplete |
| AI / collision | No changes | No fresh source-supported defect established in this graphics-scoped pass |

## Test and rights boundary

See `R187_VERIFICATION.md`. Do not describe the completed regression as a complete Jaguar hardware emulator, cycle-accurate title rendering, or an interactive campaign playthrough. Only original player-provided `AVP.jag` is used locally; it is excluded from distributed artifacts, along with extracted art and recorded footage.

---

# Alien vs Predator (1994 Atari Jaguar) — PC port R187

**Baseline:** R186 (RE #19). **Date:** October 23, 2026. **Scope:** Original title Object Processor background aperture and companion Credits backdrop.

## What changed

The retail ROM contains FILES 208 as a **320×280** JagPEG raster. The original `MJP::Make_TLi()` installs its 320-wide, 280-high unscaled bitmap object at `Zero_Line`, the start of the original frontend's display list. The native PC frontend uses a 320×240 visible surface, so this object is clipped at the bottom; there is no original source-side vertical centering. R186 instead cropped 20 source lines at the top and 20 at the bottom, shifting the background art relative to independently moving ALIEN and PREDATOR objects, menu text, and legal notice.

`src/jaguar/avp_gpu/frontend_renderer.c` now displays the **top 240 source lines** when the **TITLE** background is specifically FILES 208's 320×280 image. The hosted **CREDITS** mode, which reuses the title background, uses the same source origin. All other frontend pictures keep their existing geometry. This edit changes neither title sprite scaling nor the still-unverified GPU CROSS geometry; it is not an attempt to guess the original decompress-to-object buffers.

The source was compared with the user's BigPEmu recording `2026-10-21 15-37-18.mp4`: masked areas of the settled title picture favored source-line offset 0 over offset 20. This is supporting visual evidence, **not** a pixel-identical original output reconstruction or an all-phase video A/B certification.

`tests/test_frontend_renderer.c` checks the top and near-bottom sampled source rows for TITLE and the source origin for the CREDITS base. The test **fails on unchanged R186** at the title top-row assertion and passes on R187 under GCC and Clang. No original `src/game` files are modified.

## Scope and boundaries

- Exact 4 MiB retail cartridge SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`.
- Recovered RDB.TEXT (99,072 bytes) and RDB.DTA (60,248 bytes) identities remain established in the R182–R186 crosswalk and were not altered.
- **Still unverified:** original title image-buffer construction for scaled FILES 205/207, GPU CROSS scanline behavior, credits authored OP scroll, original title legal glyph layout, exact pause/computer OP compositing, and JERRY waveform accuracy. Those were deliberately not rewritten without original hardware evidence.
- Both strict GCC/Clang Linux Release builds and broad test suites excluding the historical three expensive tests pass. GCC/Clang frontend UBSan passes. Both retail-ROM/Xvfb startup checks reach the frontend before an external eight-second SIGTERM. Neither is a gameplay or pixel-level original-hardware comparison.
- Complete current source, both Linux executables mode 0755, research, licenses/credits and cumulative R001–R187 addendum only. **No ROM, extracted assets, recording, older release archives, Ghidra databases, or saves** are bundled.
