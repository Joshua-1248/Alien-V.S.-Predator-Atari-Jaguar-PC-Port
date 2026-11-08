# R157 — JERRY type-44 envelope register arithmetic

Atari's JERRY DSP is a 32-bit RISC processor with 32-bit arithmetic registers, extended 40-bit multiply/accumulate, signed saturation commands and modulo addressing. Its signed register arithmetic is not equivalent to C signed arithmetic once it crosses the signed representable limit. The host audio path is a behavioral reconstruction and does not execute the original DSP instruction stream, but it must still make the finite-width register behavior deterministic.

`src/jaguar/host/fullsynth_host.c`, `synth_sample_compressed()`, type 44: the envelope state `p[11]` and slope `p[12]` are 32-bit DSP words. Prior C: `cur+=slope` invokes undefined behavior at signed overflow; `dest-iv` can do the same. R157 uses an unsigned 32-bit sum and difference. Conversion from the wrapped positive `uint32_t` word to signed is an explicit subtraction of 2^32 in 64-bit arithmetic, so no implementation-defined out-of-range cast is required. Ordinary non-overflow input produces identical mathematical sums and sign checks.

Reproducer: synthesize a type-44 descriptor with envelope `p[11]=0x7fff0000`, slope `p[12]=0x20000` and centered pan, feed a synthesized constant sample. R156 under `-fsanitize=signed-integer-overflow` reports runtime error `2147418112 + 131072`, R157 produces two deterministic negative output frames and passes with both GCC and Clang. No original AVP data, JERRY waveform data or other copyrighted payload is included.

Still open: exact original DSP source branch reconstruction, 40-bit MAC sequence, internal waveform ROM characteristics, envelope modulation/pitch/timbre and audio/video lockstep. The synthetic result demonstrates defined JERRY-style 32-bit wrap behavior for this path, not sample-identical character-selection music.

Sources: https://asterick.github.io/jag-docs/jerry/dsp.html and https://asterick.github.io/jag-docs/jerry/audio.html.
