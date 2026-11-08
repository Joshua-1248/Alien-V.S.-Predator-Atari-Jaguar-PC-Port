# R167 — Original JERRY MAC RESMAC register-boundary research

**Date:** October 22, 2026. **Baseline:** R166. **Scope:** Hosted FullSynth arithmetic and synthetic tests only. `AVP.jag` is the only required game ROM.

## Original-source evidence

The frozen-exact reconstructed original FullSynth program, `src/frozen_exact/FULSYN_OJ_frozen_exact.s` in the user's original Jaguar decompilation, and the recovered RE #15 Ghidra R89 analysis agree on the following instruction chains:

- `$F1B36A IMULTN`, `$F1B36C IMACN`, `$F1B36E RESMAC`, `$F1B372 SHARQ #8`: FM envelope interpolation.
- `$F1B494 IMULTN`, `$F1B496 IMACN`, `$F1B498 RESMAC`, `$F1B49A SHARQ #8`: FM oscillator interpolation.

The Atari Jaguar Tom & Jerry technical reference establishes that `IMULTN` and `IMACN` use **signed low 16-bit input halves** and accumulate in a **signed 40-bit MAC**, whereas `RESMAC` transfers only the **low 32-bit result** to a general-purpose DSP register. The `SHARQ` that follows shifts this signed 32-bit register, not the full 40-bit MAC. `SAT32S` is a distinct instruction and must **not** be substituted for `RESMAC` in this sequence.

Reference: https://asterick.github.io/jag-docs/jerry/dsp.html and https://asterick.github.io/jag-docs/reference/risc-instruction-set.html

## Previously incorrect host semantics

R166 used `int64_t` multiplication/addition and immediately called `avp_jerry_asr64(sum,8)` in the FM interpolators. That omits the 32-bit truncation at `RESMAC`. With two products of `-32768 * -32768`, the 40-bit accumulator holds `+2147483648`; the low 32-bit register is `0x80000000`, so signed shift by eight gives `-8388608`. Shifting the untruncated 64-bit positive accumulator gives `+8388608` instead.

## R167 source change

- `include/jaguar/host/dsp_arithmetic.h`: implements `avp_jerry_mac2_resmac_asr`, which calculates the two signed products in a sufficiently wide host accumulator, returns the signed low 32-bit register value explicitly, then sign-extends the requested arithmetic right shift. No host signed overflow, implementation-defined narrowing, or ROM data required.
- `src/jaguar/host/fullsynth_host.c`: uses that operation for the two instruction-proven FM two-tap MAC interpolation sites. It retains original operand-width conversions and coefficient `255 - fraction` from R162–R164.
- `tests/test_jerry_asr.c`: adds artificial positive-overflow, negative-range, normal, and signed-shift cases. No extracted samples or wave-ROM bytes.

This patch **does not** implement full DSP register banks, interrupt timing, original waveform ROM tables, or entire 40-bit MAC state across arbitrary instruction sequences. It models precisely the two-product chains established above. In the current normal oscillator and envelope interpolation, the coefficient magnitude is at most 255, so the two-term sum cannot reach the `RESMAC` overflow boundary. Therefore **no audible music change is predicted for the current FM path**. Its value is hardware correctness and explicit state truncation for future instruction-level reconstruction.

## Verification and preservation

- GCC and Clang strict Linux Release builds: PASS.
- GCC full test coverage: PASS using normal suite through Predator video checks followed by three unchanged remainder groups, following a full-suite command timeout.
- Clang complete unchanged-test coverage: PASS in five sequential groups (two early and three later).
- GCC/Clang focused synthetic MAC tests and UBSan: PASS.
- Architecture ownership and public-asset audits: PASS.
- Original 33 gameplay-source files: SHA256-identical to R166.
- Real `AVP.jag`/BigPEmu PCM sample equality: **not verified**. Ghidra headless was **not rerun**; previous RE #15 Ghidra records and the public exact frozen original code provide evidence.

No additional copyrighted files, ROMs, waveform dumps, extracted media, diagnostic instrumentation, or gameplay changes.
