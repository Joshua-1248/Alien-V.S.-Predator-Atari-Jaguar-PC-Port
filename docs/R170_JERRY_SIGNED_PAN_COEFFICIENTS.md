# R170 — Original JERRY signed stereo-pan coefficient fidelity

**Date:** October 22, 2026. **Baseline:** R169. **Scope:** FullSynth host mixer and generated-audio regression only.

## Original instruction proof

The original retail FullSynth DSP program is preserved as reconstructed object words in the user's public original-game decompilation (`src/frozen_exact/FULSYN_OJ_frozen_exact.s`; this is a reconstruction of code bytes, not Atari's original source spelling). The earlier R159 Ghidra register crosswalk and R160 source audit document the pan tails at DSP `$F1B2D2..DA`, `$F1B266..6E`, `$F1B38E..96` and `$F1B450..58`.

For every synthesized sample, JERRY loads pan as a **32-bit descriptor register**, computes the other channel coefficient as `$7FFF - pan` by a 32-bit `SUB`, and executes signed 16x16 `IMULT` on the *low signed 16 bits* of each coefficient before adding the unshifted products to the stereo accumulators. The fact that actual retail music controller values are normally constrained to 0..$7FFF does not change the instruction semantics.

For the DSP FM tail, the recovered original words at offsets `$394` through `$39C` encode the complementary pan subtraction and signed multiplies (`$1274` `SUB r19,r20`; `$45B3` `IMULT r13,r19`; `$45B4` `IMULT r13,r20`, followed by accumulator adds). R169's host incorrectly discarded bit 15 of the descriptor with `p[19]&0x7fff` before computing the complement and multiplication. It therefore produced a positive full-right or hard-left sample where the original signed-16 DSP coefficients were negative.

Reference: https://github.com/Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation/blob/main/src/frozen_exact/FULSYN_OJ_frozen_exact.s and the Jaguar RISC ISA https://asterick.github.io/jag-docs/reference/risc-instruction-set.html . No original DSP image or copied emulator implementation is included.

## R170 changes

- `src/jaguar/host/fullsynth_host.c`: both `music_mix_one()` and the sound-effect branch of `avp_jaguar_fullsynth_mix()` preserve the full 32-bit pan word until the DSP's signed-low-16 `IMULT` operand boundary. The `0x7fff - pan` subtraction remains a defined unsigned 32-bit word subtraction, as on JERRY.
- `tests/test_host_audio.c`: adds two generated FM descriptors whose pan words `0x00008000` and `0x12348000` have identical low signed halves. Both must produce identical negative stereo PCM; the unmodified R169 mixer fails, R170 passes. Existing positive, normal-pan audio samples still match exactly.
- No changes to original gameplay, scores, patches, oscillator waveforms, original sample decoding, frontend, menu transitions, title timing, audio master gain, PC controls, or previous releases.

## Verification, limits, and next work

- Strict GCC and Clang Release builds: PASS.
- Focused audio regression GCC and Clang: PASS; identical new test against unmodified R169: FAIL as expected.
- GCC and Clang focused UndefinedBehaviorSanitizer: PASS.
- GCC regression runner: all checks reached before the very slow geometry test passed; the remainder of the suite was split into two groups and passed (with the geometry and projectile cases not completed in this session).
- Clang regression runner: early tests and two additional groups passed, with final suite marker printed for the continuation group; the long-running geometry/projectile tests were not completed in this session; a subsequent split rerun confirmed the late overlay and remaining controls tests.
- Neither complete unmodified regression runner reached its final success marker before timeout here. **Do not interpret these timeouts as PASS**. The two long-running gameplay tests contain unchanged source and passed as part of the archived R169 baseline, but were NOT newly verified for R170 in this environment.
- All 31 original gameplay/platform `.c` translation units in the `avp_c` CMake target were compared bytewise with R169 and remain identical. Native ELF binaries are marked executable.
- No new Ghidra run and no BigPEmu-vs-PC waveform/PCM equivalence demonstration. The current mathematical host waveforms remain approximations.

Only the original user-provided `AVP.jag` is required to run the game. It is NOT distributed with these packages, nor is any `jagwave.rom`, proprietary hardware waveform ROM, extracted music, or original art.
