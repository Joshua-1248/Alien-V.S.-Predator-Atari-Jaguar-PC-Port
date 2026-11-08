# R230 — Original FullSynth stereo master-gain ROM audit

Research-only successor to R229. The original 2,952-byte DSP executable embedded inside the user's retail 68000 program confirms `$F1BB40` supplies the common stereo master volume. The output sequence reads the word once, applies `IMULT` to both signed-low-16 channel accumulators, and shifts the result. The full CPU-side 132-callback fade was established in R229. The current PC mixer already implements the equivalent gain placement, so no unjustified production patch is applied.

New source: `tools/audit_retail_dsp_master_gain_r230.py`, `R230_RETAIL_DSP_MASTER_GAIN_ROM_AUDIT.md`; updated complete cumulative addendum. Unchanged: all production source, original game mechanics, rendering, audio, controls, saves and existing tests. Both Linux GCC/Clang executables and original creators' credits/licenses are included; commercial ROM, DSP bytes, wave ROM, saves, debug instrumentation and older ZIPs are excluded.
