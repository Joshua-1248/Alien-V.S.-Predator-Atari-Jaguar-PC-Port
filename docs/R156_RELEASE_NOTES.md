# Alien vs. Predator Atari Jaguar PC Port — R156

## Change
- Widened the hosted JERRY FullSynth music and sound-effect stereo accumulators from signed `int` to `int64_t` before the existing final 16-bit saturation, preserving all sample math and playback parameters.
- Added a synthetic 16-voice overlapping playback regression with positive and negative output saturation. This fixture creates its own data at runtime; no copyrighted ROM/audio material is embedded.
- The original gameplay and MJP code are unchanged. No external ROM or firmware is required; `AVP.jag` remains the only required input.

## Fidelity boundary
The actual Jaguar DSP has a 40-bit signed MAC accumulator, which is **not** the same as saying every C mixer operation now emulates DSP instructions. This is a narrow arithmetic-width correction. Synthesized waveform shapes, FullSynth patch/envelope fidelity, exact JERRY DSP sequencing and character-selection timbre remain unverified and unmodified. The release build is not a hardware-equivalence claim.

Atari references: https://asterick.github.io/jag-docs/jerry/dsp.html and https://asterick.github.io/jag-docs/jerry/audio.html . No hardware ROM, game payload, or third-party emulator code is bundled.
