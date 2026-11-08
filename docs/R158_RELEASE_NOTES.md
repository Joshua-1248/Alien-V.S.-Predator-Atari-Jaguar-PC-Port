# Alien vs Predator (Atari Jaguar) PC port — R158

## Scope

- Continue Jaguar FullSynth/JERRY arithmetic reconstruction using only the user-supplied `AVP.jag` as runtime ROM input. No extra hardware ROM files, proprietary media, or borrowed emulator code.
- Replace implementation-defined signed right shifts of potentially negative, 64-bit intermediate fixed-point values in the behavioral FullSynth waveform interpolation, compressed-sample envelope, and FM envelope/modulator branches with a deterministic arithmetic-shift helper.
- Use unsigned magnitude and integer division by powers of two to reproduce **sign extension and floor rounding**; negative `INT64_MIN` is supported without negation overflow. The replacement avoids relying on GCC/Clang treatment of signed `>>`.
- Add a synthetic, asset-free `test_jerry_asr` regression against an independent division-based oracle. It exercises boundary values and 20,000 deterministic signed inputs over shifts 0–64. Run it in the standard GCC/Clang regression suite.

## Fidelity scope and limits

The Jaguar DSP's `SHA` and `SHARQ` instructions sign-extend negative values. This source correction formalizes signed fixed-point shift behavior in our *hosted behavioral model*. It does **not** prove that the original FullSynth DSP program uses the same operation at every modeled stage. Existing Linux compilers already performed sign-extending shifts; **audible output is expected to be unchanged on Linux**. This revision does not tune or verify the character-selection music timbre, waveform tables, envelope sequencing, 40-bit MAC timing, or PIT clock accuracy. The exact original DSP instruction order remains outstanding.

The code is confined to `src/jaguar/host/fullsynth_host.c`, new `include/jaguar/host/dsp_arithmetic.h`, synthetic test `tests/test_jerry_asr.c`, and the test runner. The 33 original `src/game/` files are unchanged from R157. No gameplay, music score data, patch parameter, title frontend, or saved-game behavior is altered.

## References (study only; no code copied)

- Atari Jaguar Technical Reference, `SHA` and `SHARQ` definitions: https://asterick.github.io/jag-docs/reference/risc-instruction-set.html
- JERRY DSP registers and signed arithmetic: https://asterick.github.io/jag-docs/jerry/dsp.html
- Jaguar audio subsystem and timers: https://asterick.github.io/jag-docs/jerry/audio.html

## Verification

See `R158_VERIFICATION.md` for actual build/test results. No direct `AVP.jag` source-to-BigPEmu sample comparison was performed in this revision.
