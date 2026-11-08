# R224 — Retail PRE_LIST branch phrase and TOM display bounds audit

**Date:** 2026-10-27. **Baseline:** R223. **Scope:** original 68000 branch phrase decoding and logical scanline aperture only. **Production source remains unchanged.**

## Verified cartridge evidence

The verified retail ROM is privately supplied and is not redistributed.

**Canonical SHA-256:** `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`.

The `tools/audit_retail_prelist_branch_conditions.py` offline audit verifies **44 independently keyed, non-overlapping original 68000 instruction groups** directly against the decompressed 99,072-byte retail program. It checks the original `$0090E0–$009108` SetScreen display-bound stores, `$009210–$00923E` PRE_LIST builder, `$00927A–$00929C` branch phrase packing, and `$009308–$00931C` STOP builder. This corrects an audit-coverage weakness in the earlier R223 script: its dictionary reused three instruction addresses, silently replacing checks for their earlier adjacent instructions. The old script remains preserved as historical evidence; the new one explicitly checks all relevant individual boundaries.

The original branch writer encodes a LINK using `MOVE.L A1,D2`, `SWAP D2`, `ROL.L #5,D2`, and its two successive longword writes; inserts the condition through `SWAP`/`LSR.L #2`; inserts the half-line position using `LSL.W #3`; and sets `TYPE=3`. It produces these original phrases:

| Video | Branch address | Two 32-bit phrase words in CPU memory order | Decoded target | YPOS | CC |
| --- | --- | --- | --- | --- | --- |
| NTSC 320×240 | `$7548` | `$0000000E $AB0040E3` | `$7558` | 28 | 1 |
| NTSC 320×240 | `$7550` | `$0000000E $AC004FE3` | `$7560` | 508 | 1 |
| PAL 320×280 | `$7548` | `$0000000E $AB004163` | `$7558` | 44 | 1 |
| PAL 320×280 | `$7550` | `$0000000E $AC0052E3` | `$7560` | 604 | 1 |

Per Atari Jaguar TOM OP documentation, **CC=1 means branch if `YPOS > VC`**. This gives three paths with the unchanged static OLP starting at `$7548`:

1. `VC < VDB`: first branch takes `$7558`, which contains the original STOP record `(0,4)`.
2. `VDB <= VC < VDE`: first branch falls through and the second branch takes `$7560`, the dynamic OP object-list head.
3. `VC >= VDE`: neither branch takes; traversal falls through to STOP at `$7558`.

`SetScreen` stores `VDB = video_vcenter - display_height` at `$2EE94`, and `VDE = video_vcenter + display_height` at `$2EE98`, in half-lines. Original MJP configuration yields NTSC `0x001C..0x01FC` (240 noninterlaced lines) and PAL `0x002C..0x025C` (280 lines). All even VC values in the 11-bit field and four boundary cases for each mode are checked. These are **logical OP branch decisions**, not empirical scanline timing measurements.

Technical reference: https://asterick.github.io/jag-docs/tom/object-processor.html (BRANCHOBJ CC table and OLP word swap), cross-checked against original CPU instruction packing.

## Production preservation and limitations

No changes to original simulation, weapon state, HUD, title visual effects, user controls, sound/music, saves, existing tests, or shaders. The offline ROM audit is not production instrumentation. The working PC renderer retains its prior unverified crossfade presentation.

**Unproven:** exact physical OLP fetch timing, CPU VBlank execution point relative to scanline reads, GPU stage publication and shared-bus arbitration, pixel-exact 16-stage transition and final real-hardware appearance. The verified branch conditions alone do not authorize replacing the current crossfade with a claimed 1:1 effect.
