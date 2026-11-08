# R223 — Retail Jaguar PRE_LIST and VBlank callback dispatch audit

**Date:** 27 October 2026. **Parent:** R222. **Scope:** read-only original-ROM instruction audit and documentation. All production gameplay/render/audio/input/save code remains untouched.

## Evidence

Input is the user-owned 4,194,304-byte `AVP.jag`, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Retail gzip image at ROM `$003804` decompresses to the original 99,072-byte 68000 program at CPU `$008000`. The script `tools/audit_retail_prelist_vblank_dispatch.py` checks 39 original CPU opcode windows and control-flow targets; no original images, code bytes, or ROM files are distributed.

## Original VBlank interrupt path

* CPU `$00918A`: original `MOVE.L` installs handler `$0091B6` in low-memory vector `$00000100`.
* Handler `$0091B6`: advances counter `$0002EE84`, clears `$0002EE88`, saves working registers, loads callback pointer from `$0002EE80`, and invokes its nonzero target through `JSR (A0)`.
* If no callback exists, `$0091D8` loads A0=`$00007560` and branches to the STOP-record writer at `$009308`, which writes two 32-bit words `(0,4)` at that location. The branch is specifically to the *full writer* at `$9308`, not only its second-longword opcode `$930A`.
* Handler then restores registers, writes `$0101` to Jaguar `$F000E0`, clears `$F000E2`, and returns using `RTE` at `$0091F4`.
* This proves the CPU callback and interrupt flow, **not** hardware interrupt scanline timing or the exact moment the TOM Object Processor reads shared RAM.

## PRE_LIST created by the original 68000

Original PRE_LIST builder begins at `$009210`. It positions A0 at `$7548` and calls the branch-phrase builder at `$927A` twice:

| Constructed phrase address | Original target / source | Source instructions |
| --- | --- | --- |
| `$7548` | Branch target `$7558`; video boundary from `$2EE94` | `$9216..$9226` |
| `$7550` | Branch target `$7560`; video boundary from `$2EE98` | `$9228..$9238` |
| `$7558` | STOP phrase `(0,4)` | builder call `$923A` to `$9308` |

The cursor is then `$7560`. This is a **branch/STOP staging list**, not a second full-width screenshot buffer. R222's word-swapped OLP `$75480000` points TOM at this structure. The exact branch phrase packing code at `$927A..$929C` is authenticated; no hardware-level interpretation of the per-scanline branch conditions is invented.

## Selection of 6/8/10-object list-copy callbacks

The original ROM installs a callback by writing a handler address to shared `$2EE80`. Verified 68000 instruction sites include:

| Install site | Installed callback | Meaning from prior R222 evidence |
| --- | --- | --- |
| `$1BE00` | `$1BEC2` | Frontend callback (separate from the three list-copy routines) |
| `$1BFF2` | `$1E804` | 10-record list path |
| `$1C08C` | `$1E4D0` | 8-record list path |
| `$1C130` | `$1E34A` | 6-record list path |
| `$1C1C0` | `$1E4D0` | 8-record list path |
| `$1C25A` | `$1E34A` | 6-record list path |

The ROM demonstrates *which callback targets are installed at those sites*. It does **not** establish which site is reached in every retail frontend scene without a live execution trace. R222 established that each corresponding list-copy function sets `$7FA8 = 1` only after copying its records; R223 checks those original write opcodes again. Thus the VBlank IRQ, callback choice, list copy, and GPU wake-up can now be linked through authenticated instructions.

## Limits and next gate

These findings are **source-proven CPU control flow, not 1:1 visible-frame parity**. Unverified: live interrupt scheduling, branch predicate evaluation per TOM video counter, OP traversal at each scanline, GPU/Blitter DMA contention and whether scene buffers are complete before the hardware samples them. Do not replace the PC crossblend with a conjectural one-frame timing rule. Next verify branch phrase bitfields and the original TOM VC/VDB handling against a cycle-aware trace or original/emulator video, and recover the frontend callback `$1BEC2` where relevant.

No change to production `src/`, `include/`, or `tests/` is made. Preserve user-confirmed weapon HUD, Marine/Alien equip, audio, analog elevator, frontend and save behavior. Keep commercial ROM and proprietary artwork/audio out of releases.
