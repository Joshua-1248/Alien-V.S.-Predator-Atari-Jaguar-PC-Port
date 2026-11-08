# R158 — Deterministic signed fixed-point shifts for JERRY FullSynth

The hosted FullSynth code performs interpolation and envelope multiplication on signed fixed-point samples. For positive values, ordinary right shift and truncating division are equivalent, but for negatives they differ: signed arithmetic shift rounds toward negative infinity. For example, `-129` arithmetically shifted 7 positions produces `-2`, whereas C's integer division `-129 / 128` produces `-1`.

The Jaguar RISC instruction family explicitly defines `SHA`/`SHARQ` to shift the sign bit into vacated positions. ISO C99 leaves the result of right-shifting a negative signed integer implementation-defined. R158 defines `avp_jerry_asr64` using an unsigned magnitude, a checked shift count, and the quotient of `(magnitude+2^shift-1)/2^shift`. The result is negated only after the quotient is reduced to the signed range. Special cases are zero shift and shift >= 64. This is a fully defined host implementation for 64-bit fixed-point intermediates. It is not a generic Jaguar opcode emulator.

The helper is applied to negative-valued fixed-point calculations in `host_wave_sample`, `synth_sample_compressed`, and `synth_sample_fmenv`. Phase additions and wrapping of 32-bit DSP register fields remain unchanged. Mixing, stereo pan, volume saturation, JERRY timers, internal waveform approximations, and original score/pitch/patch selection remain unchanged.

The synthetic regression uses an independent signed division/remainder oracle, including `INT64_MIN`, the opposite limit, and random test cases. A compiler-independent output for negative arithmetic shifts removes a portability risk, but does not establish sample-exact emulation of the proprietary original DSP program.

Sources: https://asterick.github.io/jag-docs/reference/risc-instruction-set.html ; https://asterick.github.io/jag-docs/jerry/dsp.html .
