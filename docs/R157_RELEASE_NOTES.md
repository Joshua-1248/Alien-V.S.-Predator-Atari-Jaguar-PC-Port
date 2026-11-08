# Alien vs. Predator (Atari Jaguar) PC port — R157

## Correct 32-bit DSP envelope register wrap

In the AVP.jag-only FullSynth host implementation, the compressed sample + envelope path (module type 44) previously computed `cur += slope` using signed 32-bit C values and also subtracted two signed values for an envelope sign check. Both operations can invoke C's undefined signed-overflow behavior. JERRY's DSP uses 32-bit registers with deterministic modulo-2^32 addition/subtraction before inspecting the resulting sign bits. R157 now performs the additions and subtraction in unsigned 32-bit arithmetic and explicitly interprets the wrapped sum as a signed 32-bit register value. The envelope's existing interpolation, note parameters, volume, pitch, sample selection, patch descriptors, loop/release decisions, pan, voice allocation and stereo mixing remain unchanged.

A new synthetic type-44 envelope test enters the signed-overflow boundary and checks audible output on successive samples. It passes with both GCC and Clang. GCC's `-fsanitize=signed-integer-overflow -fno-sanitize-recover` run detects the original R156 error at `fullsynth_host.c:387` (2147418112 + 131072) and passes R157. The fixture synthesizes all its data; it embeds no music, wave ROM or cartridge assets.

## Verification limits

This corrects one DSP-register arithmetic mismatch, not the complete JERRY program. FullSynth still uses mathematical approximations of the eight waveforms; the remaining five source-program indirect branches and the exact timbre are unverified. No other commercial ROM, hardware firmware or user-supplied wave file is needed. `AVP.jag` remains the only required ROM. Neither the CPU/game source, score, instrument parameters, audio clock, nor TOM presentation is changed.

References (behavioral specifications, not copied code): Atari Jaguar Software Reference, [DSP arithmetic](https://asterick.github.io/jag-docs/jerry/dsp.html) and [JERRY sound interface](https://asterick.github.io/jag-docs/jerry/audio.html).
