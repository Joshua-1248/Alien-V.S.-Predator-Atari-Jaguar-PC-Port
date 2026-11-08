# Alien vs Predator (1994 Atari Jaguar) PC port — R174

**October 22, 2026 — SELECT selected-portrait initial OP scanline continuity**

**Baseline:** R173. **Scope:** hosted SELECT rendering and its existing framebuffer regression only.

## Root cause

The final Jaguar character carousel SOBJ stores the selected card's source bitmap with the original SCBITOBJ 3.5 fixed-point scaling. For a 320x240 decoded portrait the CPU-authored OP uses 239 source lines; at scale 16/32 it draws **120 destination lines**, sampling exactly source lines **0, 2, 4, ..., 238**.

At the instant of the `SGCF` selection crossfade, the PC port reused the last SOBJ card rectangle but resampled its bitmap as `floor(output_line * 239 / 120)`, producing **0, 1, 3, ..., 237**, even with no zoom movement and crossfade phase zero. Thus the selected image visibly changed its vertical texture rows at the transition boundary, before the original 50-tick approach began. This discrepancy is intrinsic to two existing rendering paths and does not require inventing any Jaguar gameplay change.

## Narrow correction

`src/jaguar/avp_gpu/frontend_renderer.c::frontend_blit_select_zoom_cross` now recognizes the exact SGCF tick-zero case when the chosen valid SOBJ slot's published coordinate, `scale` and 239-line extent correspond to the outgoing 320-pixel carousel picture. For **that first frame only**, it uses the same original OP sampling `floor(n*32/scale)` as the immediately previous SELECT card, preserving each source scanline and its final partial repetition. The remainder of the 50-VBlank hosted GPU zoom interpolation, RGB16 cross arithmetic, subsequent SGFI fade, R173 450-VBlank presentation hold, Fire gate, original dialogue, title screen, music fade and gameplay all remain unchanged.

This establishes a specific first-frame OP handoff invariant. The **exact Jaguar GPU CROSS expansion curve beyond tick zero is still not reconstructed** and no 1:1 per-pixel claim is made for those later frames or the JagPEG decoder.

## Regression and preservation

`tests/test_frontend_renderer.c` uses a synthetic 320x240 portrait with distinctive 8-bit-safe source rows, caches the last carousel card's 120 displayed rows, then starts SGCF and compares **every scanline** before any `TICK`. It fails with unchanged R173's renderer at the first mismatch and passes on R174. GCC/Clang strict Release builds, focused MJP/music/frontend renderer tests, and both frontend UBSan test runs passed. All **33** original-derived C files under `src/game` are byte-identical to R173. No code structure changes, commercial assets, ROMs, extracted graphics/audio, or wave-ROM dependency were added. Full gameplay regression and interactive screenshot A/B were not run; see `R174_VERIFICATION.md`.
