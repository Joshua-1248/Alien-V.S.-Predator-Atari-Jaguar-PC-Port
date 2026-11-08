# R156 — JERRY arithmetic-width review

## Hardware-derived contract
JERRY's `IMULTN`/`IMACN`/`RESMAC` use an extended 40-bit signed accumulator; `SAT32S` observes the overflow bits, and `SAT16S` converts a signed 32-bit operand to signed 16-bit saturation. The DAC is stereo 16-bit via the synchronous serial interface. These specifications require guarding intermediate precision and saturation points when reconstructing the original DSP program.

## Concrete change
In `fullsynth_host.c`, `music_mix_one` and the aggregate left/right mixer operands now use `int64_t`. The C code formerly narrowed each contribution to signed 32 bits before summing all audible voices. The existing per-voice calculations and master-volume shift remain; the only intended behavioral difference is avoiding an overflow artifact under excessive simultaneous amplitude. The synthetic fixture mixes a full set of 16 voices from locally generated samples to exercise signed 16-bit clipping on both channels.

## Still unresolved
This mixer change is not an instruction-accurate emulation of Jaguar 40-bit MAC or the internal ROM waveform tables. In particular waveform generation, DSP program indirect branches, exact envelope/modulator arithmetic, PIT timing and authentic character-select instrumentation are not proven identical to the original. No speculative oscillator reshaping was introduced. A `AVP.jag`-derived audio capture and reference comparison are still needed to support timbre modifications.

## Citations
Atari Jaguar Developer Reference, Jerry DSP https://asterick.github.io/jag-docs/jerry/dsp.html ; JERRY audio https://asterick.github.io/jag-docs/jerry/audio.html ; Jaguar TRM https://hillsoftware.com/files/atari/jaguar/jag_v8.pdf . These are research links only. No copyrighted samples, firmware, or third-party emulator source are copied.
