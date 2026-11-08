# R168 — JERRY compressed-envelope signed-operand width

**Date:** October 22, 2026. **Baseline:** playable R167. **Scope:** hosted FullSynth compressed-sample arithmetic and synthetic tests only. `AVP.jag` remains the only required ROM.

## Original-code evidence

The original frozen-exact FullSynth DSP program is preserved as instruction words in the user's public Atari Jaguar original decompilation at `src/frozen_exact/FULSYN_OJ_frozen_exact.s`. Older RE #15 Ghidra research mapped the type-44 compressed-sample-with-envelope handler to `$F1B398` and the original 80-byte (20-word) voice descriptors.

The original handler's envelope and final volume sequence is:

- `$F1B442`: `IMULT r3,r13` — multiply **signed low 16 bits** of the shifted envelope and interpolated sample register.
- `$F1B444`: `SHARQ #15,r13` — shift the resulting signed 32-bit register.
- `$F1B446`: `IMULT r5,r13` — multiply **signed low 16 bits** again for output gain.
- `$F1B448`: `SHARQ #15,r13` — shift before panning.
- `$F1B44A`: `LOAD (r15+18),r19` — physical descriptor word 19, the original pan coefficient after the established dispatch delay-slot adjustment.

The plain compressed handler similarly uses `IMULT` for the final volume stage at `$F1B2CC`; the latter does not change normal output because its operands already fit signed 16 bits. The Jaguar technical reference states that `IMULT` multiplies *the signed bottom 16 bits of both operands* and writes a signed 32-bit product; the following shift is arithmetic.

Reference: https://asterick.github.io/jag-docs/reference/risc-instruction-set.html

## Verified host mismatch and correction

R167's type-44 path did the equivalent of `((int64_t)sample * (int32_t)(envelope_phase >> 15)) >> 15`. That leaks high bits from the envelope register into multiplication. R168 passes both operands through the existing defined `dsp_low16_signed` conversion at each original `IMULT` stage. The helper was moved above the two sample handlers with no functional modification. The synthesized waveform tables, sample decoder, music score, pitch, descriptor layout, stereo mixer, DAC saturation, and gameplay code remain unchanged.

A synthetic pair of descriptors uses envelope accumulators `0x20000000` and `0xA0000000`. Following the original shifted-register step, the corresponding signed values are +16384 and −49152: their **low signed 16 bits are identical (+16384)**. The original DSP therefore produces identical audio. R167's full-width multiply differs; R168's two PCM arrays are byte-identical and nonzero over 64 frames under GCC and Clang.

An R157 test previously asserted negative PCM after a 32-bit envelope wrap. That older expectation was an artifact of full-width host multiplication: the correctly truncated low-16 envelope gain is 2 or 6 in the first two synthetic frames and correctly quantizes to silence. The fixture retains the wrap/sanitizer coverage and now expects the original instruction's output, rather than the old host bug.

## Limits and preservation

- This correction changes output only when the original DSP's 16-bit multiplication would discard significant upper bits. No real-game ROM PCM or BigPEmu sample-equivalence comparison was run.
- The authentic JERRY hardware waveform table values, DSP interrupt cadence, register-bank switching, and full sound parity remain unverified. The host waveforms remain generated mathematical approximations; no `jagwave.rom` or extra ROM requirement is added.
- Older RE #15 Ghidra R89–R93 findings were reused; Ghidra was not rerun for this pass.
- The test data is fully synthetic. No commercial soundtrack, game artwork, proprietary DSP binary, hardware ROM, or copied emulator code is included.
- All original 33 `src/game/` files are byte-identical to R167. Original project credits, licenses, and current Linux feature set are retained.
