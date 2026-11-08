# R169 — JERRY FullSynth FM envelope/volume multiplication order

**Date:** October 22, 2026. **Baseline:** playable R168.
**Scope:** original FM module type 4, host-side arithmetic and synthetic audio regression.
No original gameplay code changed. `AVP.jag` remains the sole required game ROM.

## Original-source proof

The user's preserved Jaguar source reconstruction `src/frozen_exact/FULSYN_OJ_frozen_exact.s` in `Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation`, together with the earlier R89–R93 Ghidra crosswalk, establishes the original FM module's instruction order:

- `$F1B36A` `IMULTN r3,r10` and `$F1B36C` `IMACN r11,r12` interpolate the envelope. `$F1B36E` `RESMAC r13` transfers the low 32 bits and `$F1B372` `SHARQ #8,r13` produces the envelope value.
- `$F1B374` `IMULT r5,r13` multiplies envelope by volume (using signed low-16 register halves).
- `$F1B378` `SHARQ #15,r13` shifts the new envelope-volume intermediate.
- `$F1B37C` `IMULT r16,r13` multiplies the oscillator by the intermediate.
- `$F1B38A` `SHARQ #15,r13` produces the output for original pan multiplication.

The machine instruction ordering and signed-low-16 operands are essential. The R168 host instead evaluated `((oscillator * volume) >> 15) * envelope >> 15`; rounding in the intermediate means that these are not generally equivalent.

Sources: <https://github.com/Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation/blob/main/src/frozen_exact/FULSYN_OJ_frozen_exact.s> and <https://asterick.github.io/jag-docs/reference/risc-instruction-set.html>.

## Focused implementation and regression

R169 applies the volume multiplication to the interpolated envelope first, with `dsp_low16_signed()` applied to both `IMULT` inputs and `avp_jerry_asr64(...,15)` after each product. The existing separate FM feedback/modulation path is deliberately unchanged, as are the patched instrument values, authored song/score, host mathematical waveform approximations, pan, clipping, interrupt scheduler, and game files.

The new synthetic test sets sine phase to index 8, a constant envelope lookup, signed volume 10,000 and hard-right pan. It proves output PCM right=3794, left=0 for the first sample; the R168 host fails this assertion, and the R169 implementation passes GCC and Clang. This is an instruction-order and audio-output regression, not a copyrighted waveform or cartridge audio dump. Both GNU/Clang undefined-behavior sanitizer runs pass.

## Boundaries

No live `AVP.jag` versus BigPEmu audio sample comparison was conducted in this revision, so audible parity is not established. The hardware waveform table values remain mathematical approximations. The original FullSynth DSP program is not yet executed instruction-for-instruction, and register-bank/interrupt scheduling remains unverified. No Ghidra run was performed in R169: prior real Ghidra findings were used along with the user's frozen-exact source reconstruction.

Preserve all original credits and notices and do not bundle proprietary game/hardware ROMs, extracted assets or private Ghidra project databases.
