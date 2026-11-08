# R235 — Integrated original intro-CROSS CPU/ GPU / actual PC renderer comparison

**2026-10-27 · RE #21 · Parent R234.** Offline, original-program-verified research. Canonical user-owned retail ROM SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. No ROM, proprietary art/audio or original DSP binary is redistributed. **No production modifications.**

## The new part: actual intro callback arguments and CPU OP X placement

The original introductory 68000 code at `$01D846…$01DA06` contains seven calls to the original page function `$01E1F2`, which calls the picture transition `$01DF2C`. This is not merely an isolated geometry subroutine: each call is tied to its original intro text ID by an authenticated `MOVE.L #text,$4BE70` immediately before the call. At `$01D89A` the original lower bound `$4BEDC` is reset to 0. The initial 0→0 presentation sets direction `$4BED4=0` at `$01D8A4`; immediately after that transition, the intro sets `$4BED4=−1` at `$01D8B4`. The first page sets upper bound `$4BED8=10` at `$01D904`; the second page changes it to 99 at `$01D930`; the final page changes the lower bound to 10 at `$01D9E4`.

The common original GPU-completion path `$01E140…$01E158` polls `$7F80` for status 2 and executes `NEG.L $4BED4` at `$01E152`. The verified `$01DF2C` CPU code computes eight object X coordinates per transition via `X=18+4*D1` with the original signed mode/bound logic; the original Intro page function calls it synchronously before returning. The previous audits established the 16 normal GPU processing stages (1–16) and source-driven output publication; stage count **must not** be conflated with eight original `One_Tick` calls or displayed hardware scanout frames.

| Original intro page | Original 68000 call | Direction on entry | Upper/lower | Original CPU OP X, D5 = 0…7 |
|---|---|---:|---:|---|
| Initial 0→0 | `$01D8AE` | 0 | — / 0 | 58,58,58,58,58,58,58,58 |
| 0→1 | `$01D90E` | −1 | 10 / 0 | 58,58,58,58,58,58,58,58 |
| 1→2 | `$01D93A` | +1 | 99 / 0 | 18,22,26,30,34,38,42,46 |
| 2→3 | `$01D95C` | −1 | 99 / 0 | 114,110,106,102,98,94,90,86 |
| 3→4 | `$01D97E` | +1 | 99 / 0 | 18,22,26,30,34,38,42,46 |
| 4→5 | `$01D9A0` | −1 | 99 / 0 | 114,110,106,102,98,94,90,86 |
| 5→6 | `$01D9C2` | +1 | 99 / 0 | 18,22,26,30,34,38,42,46 |
| 6→0 | `$01D9EE` | −1 | 99 / 10 | 114,110,106,102,98,94,90,86 |

These are **original CPU-authored object X values**, not reconstructed pixel capture positions. The original branch/list staging and bitmap data pointer are independently audited in R221–R228. Counted against the current hosted **fixed X=18**, the original CPU X is different for **53 of the 56** original per-page CPU object updates (excluding the initial same-page 0→0 stage). This does *not* prove that 53 particular displayed video frames differ, because some object updates can be hidden by the original GPU/Blitter/OP timing.

## Real production code, not a simulated host helper

The new `tools/retail_cross_live_host_probe_r235.c` is an **offline C harness**. It links the unchanged actual `src/jaguar/avp_gpu/frontend_renderer.c`, `frontend_fade.c`, `cry.c`, `framebuffer.c`, and the separate **non-shipped** R234 original GPU packed-word reference. It creates two synthetic 192×144 RGB16 pictures, enters MJP INTRO, sends the actual production `MJPP` event, then eight actual `TICK` events and a final `MJPS`. On each tick it calls the actual `avp_gpu_frontend_present()` on a 320×228 framebuffer and checks the sampled frame.

The actual host emits phase 2,4,6,8,10,12,14,16 over the eight ticks. On all eight, the nonblack picture area on synthetic row Y=50 remains X=18…209, irrespective of original CPU direction and limits. The actual host pixels in the tested pair match the independently verified retail RISC arithmetic at phases 2,4,6,8,16, but **differ at 10,12,14**:

| Host tick / phase | Actual host packed pixel pair | Original GPU packed arithmetic applied to identical synthetic input pixels | Match |
|---|---|---|---|
| 1 / 2 | `DA495A86` | `DA495A86` | yes |
| 2 / 4 | `C28A72C8` | `C28A72C8` | yes |
| 3 / 6 | `B2CA830A` | `B2CA830A` | yes |
| 4 / 8 | `A34CA34C` | `A34CA34C` | yes |
| 5 / 10 | `938CB38E` | `8BCEC34E` | **no** |
| 6 / 12 | `7BCDCBD0` | `7C0ED390` | **no** |
| 7 / 14 | `6C0DDC12` | `644FEBD2` | **no** |
| 8 / 16 | `548FFC14` | `548FFC14` | yes |

R234's separate GPU-instruction-vs-host audit already compared **589,824** synthetic word operations on full 192×144 and 320×144 images. R235 supplies an **actual code-path confirmation**: the user-visible MJP INTRO renderer consumes the mismatching helper, and the CPU path supplies page-dependent coordinates that the host render function ignores. This is a higher-confidence concrete integration target than the general helper discrepancy alone.

## Reproduce the new audit

```bash
PYTHONPATH=tools python3 tools/audit_retail_cross_intro_live_host_r235.py \
    --rom /absolute/path/to/your/AVP.jag --source . --cc gcc
PYTHONPATH=tools python3 tools/audit_retail_cross_intro_live_host_r235.py \
    --rom /absolute/path/to/your/AVP.jag --source . --cc clang
```

The audit checks all 13 original scene direction/bound writes, original caller and text-ID relationships, shared `$1E152` direction negation, R214 CPU geometry opcodes, R221 GPU publication and phase instructions, and the actual compiled host path. It requires no runtime diagnostic patch and ships only synthetic input pixels.

## Preservation and exact implementation gate

Do **not** replace all consumers of `avp_gpu_rgb16_cross()`: the unrelated selection zoom uses different source coordinates and scale rules. Also do not force a sliding screenshot animation merely by applying the original CPU OP X list: original `$7FA4` buffer selection, Blitter row writeback, `$7FA8` GPU/VBlank handshake and TOM active-list scanout are separate observable hardware operations. These must establish *when and which* completed output is displayed on real Jaguar/validated emulator before changing production drawing. R235 contains the new offline proofs and no production behavior changes, preserving HUD, weapon switching, audio, gameplay, input, saves and pause. The separate known architecture `PSVO` guard failure remains open, not suppressed.

Other existing open gaps: waveform-ROM/sample-accurate DSP audio, video-estimated title/high-score fades, full campaign A/B on authentic Jaguar, and scanline-accurate original GPU/Blitter/Object Processor rendering. See `R235_VERIFICATION.md` for actual test and release results; passing code-path comparisons alone is not proof of whole-game 1:1 fidelity.
