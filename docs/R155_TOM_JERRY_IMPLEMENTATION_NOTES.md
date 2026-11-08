# R155 — TOM object-scale implementation audit

**ROM policy:** `AVP.jag` only.  Hardware wave ROM / game assets are neither required nor redistributed.

## Spec evidence

Atari's SCBITOBJ phrase 3 packs HSCALE (bits 0–7), VSCALE (bits 8–15), and REMAINDER (bits 16–23). Each uses 3.5 fixed point, where 32 means unscaled. At each output scanline the hardware subtracts one unit (32 in fixed-point), adding VSCALE and advancing source lines as necessary until the remainder is positive. Initialized with REMAINDER=VSCALE, the source-line index on output scanline n is `floor(32*n/VSCALE)`. The same fixed-point source step is used for horizontal sampling. The number of output lines for N source lines is `ceil(N*VSCALE/32)`. Reference: https://asterick.github.io/jag-docs/tom/object-processor.html, **SCBITOBJ**.

## R154 versus R155

R154 used `src = floor(n * source_count / floor(source_count * scale / 32))` and a floor-rounded output extent. This can spread the sampling error across the output rectangle. R155 steps at the actual object scale and retains its final partial output pixel/line. Example: 10 source lines at 21/32 = 7 outputs, sources 0,1,3,4,6,7,9. At half scale, the authored 239-line source produces 120 output lines and never accesses the extra 240th decoded row.

The same integer-ceiling extent feeds the chosen portrait's initial host zoom size; the subsequent video-derived linear motion/blend remains marked an approximation. The object's original CPU-authored x/y coordinates, source height, scale fields, object order, and phase timings are untouched.

## JERRY: no speculative timbre patch

The hosted `fullsynth_host.c` is a behavioral DSP reconstruction, not an instruction interpreter. The documented Jaguar JERRY DSP supports 40-bit MAC (`IMULTN`, `IMACN`, `RESMAC`) and signed `SAT16S`/`SAT32S`, and the I²S sample/tempo tick cadence is under programmable timers. References: https://asterick.github.io/jag-docs/jerry/dsp.html and https://asterick.github.io/jag-docs/jerry/audio.html. The port's internal mathematical oscillator tables and envelope/phase operations have not been shown sample-identical to hardware. Because the actual retail `AVP.jag` is not mounted as a verified input, no source-backed waveform/patch adjustment can yet be justified; the user-required cartridge-only synthesis remains unchanged. No copied MAME, Virtual Jaguar, or ROM data.

## Fidelity boundary

The updated SCBITOBJ sampler has focused GCC/Clang tests. That establishes the documented fixed-point behavior of these simulated objects, not complete GPU, OP, memory-bus, blitter, or DSP cycle accuracy. Live video/audio comparison remains outstanding.
