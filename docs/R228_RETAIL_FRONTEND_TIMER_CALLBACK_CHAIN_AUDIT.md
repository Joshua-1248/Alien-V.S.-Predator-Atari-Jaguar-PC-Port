# R228 — Retail VBlank timer callback chaining and restoration (RE #21)

**Date:** 2026-10-27. **Parent:** R227. **Type:** ROM-backed research/checker only. No production code, animations, audio, rendering, input or saves modified.

## Verified original instructions

Retail 4,194,304-byte `AVP.jag` SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. The checker uses the correct hash and separately verifies the original 99,072-byte 68000 program inflated from cartridge offset `$3804`.

- At `$009A5C` and `$009A80`, the 68000 invokes the pending timer wait `$009AA4`, saves the user-provided timer argument into `$2EF44`, and *only if nonzero* copies the current VBlank callback slot `$2EE80` to `$2EF40`. It then installs temporary callback `$009ACA` or `$009AF6` respectively.
- `$009AA4` checks the current callback against those two timer handlers. If neither is installed, it returns. If either is installed, it loops testing countdown `$2EF44` until that value clears. The loop is busy-wait logic, not an exact VBlank count.
- `$009ACA` reads the original longword at `$F1BB40` (JERRY DSP local RAM) and subtracts the pending timer state; its timeout path clears `$2EF44` and copies the saved `$2EF40` callback back to `$2EE80` at `$009AE2`.
- `$009AF6` reads the same JERRY DSP local-RAM longword, adds the pending state, checks the low word's sign and on timeout writes the original `$7FFF` sentinel, clears `$2EF44` and copies the saved callback back into `$2EE80` at `$009B14`. Do not reinterpret the DSP RAM word as a hardware clock register, host millisecond duration, or verified sound-volume control without tracing its DSP-side producer and consumers.
- Both handlers reach common tail `$009B24`, which loads `$2EF40`; if non-null, executes `MOVEA.L D0,A0 / JSR (A0)` at `$009B2E`. **Thus the timer callback invokes the original saved callback in the same interrupt**, including any original 6/8/10-record OP list writer. This is a verified *instruction-level chaining mechanism*, not a guarantee of the scanline or frame at which the generated list is displayed.
- Original frontend mode 1 directly installs list writer `$01E4D0` at `$01C08C`, calls `$009AA4` at `$01C096`, starts timer B using `$009A80` at `$01C0B0`, and calls `$009AA4` again at `$01C0BC`. Mode 3 also installs `$01E4D0` at `$01C1C0` and calls `$009A80` at `$01C1F4` before further setup at `$01C1FA`. Three other retail sites call timer A: `$01CE16`, `$01EF4E`, `$01F88A`.
- As R227 previously found, the frontend mode entry at `$01BEC4` stores `$7FAC` and executes `One_Tick` at `$01BEC8` **before** the subsequent mode-specific callback installation. Therefore any previously installed handler can still receive that interrupt. This statement concerns instruction order; full calling context and exact interrupt arrival remain unverified.

## New reproducible verification

`python3 tools/audit_retail_frontend_timer_callback_chain_r228.py --rom /path/to/AVP.jag` authenticates retail source and verifies both timer installers, zero-count bypasses, pending-timer wait, clock checks, both restored-callback writes, the common chained `JSR (A0)`, mode-1/mode-3 callers, timer-A call sites and pre-install `One_Tick` ordering. It checks original exact opcode bytes and branch targets. The tool is offline; not a diagnostic build, gameplay change, or derived asset package. It never publishes retail game code or graphics.

## Remaining limits

This closes a significant gap in **static callback lifetime and nesting**: the timer wrapper chains the previous callback and restores it. It does **not** establish which interrupt arrives at any particular machine cycle, actual Blitter/GPU DMA ordering, when the Object Processor samples a changed list, or the visible result of the 16 CROSS GPU stages. The existing PC RGB16 transition is still an unverified presentation approximation and must not be changed to guessed output. Preserve all original game behavior and R202–R227 user-confirmed working features. Archive only current R228 source/binaries, addendum and credits, without ROM or older release ZIPs.
