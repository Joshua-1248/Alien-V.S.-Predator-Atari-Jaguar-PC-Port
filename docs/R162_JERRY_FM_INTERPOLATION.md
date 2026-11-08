# R162 — Original JERRY FullSynth FM wavetable interpolation parity

## Original-code evidence

Original program reconstruction: `Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation`, `src/frozen_exact/FULSYN_OJ_frozen_exact.s` (reconstructed object words; not Atari-authored original source formatting).

Prior RE #15 Ghidra R89–R93 results independently identify the FullSynth image layout and the original three synthesis module entries. The FM handler starts at DSP `$F1B2DC`; original wave sampling and interpolation are visible in this original executable handler. No contemporary Ghidra executable has been run for R162; the original instruction sequence is read from the public frozen reconstruction and cross-checked against the earlier reports.

- `$F1B30C–$F1B31A` (FM carrier/modulator position handling): `MOVEFA alt_r11,r10` retrieves the original constant `$000000ff`; a working register `r12` receives the low 8 fractional bits, then `SUB r12,r10` makes **`0xff-fraction`**.
- `$F1B494–$F1B49A` (FM oscillator interpolation): `IMULTN r3,r10`, `IMACN r11,r12`, `RESMAC r0,r13`, followed by `SHARQ #8,r13`. Thus the two signed waveform samples are multiplied by the **`255-fr` and `fr`** coefficients, accumulated, and shifted right eight bits.
- The analogously structured oscillator branch repeats the same coefficient construction for modulation. All waveform samples and coefficients are small enough that the intermediate signed sums fit within 32 bits; the actual 40-bit JERRY MAC does not overflow in this operation.
- The original wave-data values still belong to JERRY hardware; `AVP.jag` alone cannot give literal hardware waveform ROM bytes. The current port continues to approximate the shape mathematically and does not require a second ROM file.

Jaguar instruction specification: https://asterick.github.io/jag-docs/reference/risc-instruction-set.html. Under the documented `IMULTN` / `IMACN` / `RESMAC` / `SHARQ` definitions, an 8-bit weight of 255 is **not** interchangeable with 256: even fraction-zero sample output differs by 1/256 of the original value. This is unlike conventional symmetric linear interpolation and must not be silently normalized to weights summing to 256.

## R161 mismatch / R162 correction

`src/jaguar/host/fullsynth_host.c::host_wave_sample()` calculated:

```
((256-fr) * a + fr * b) >> 8   [R161]
```

The instruction-derived expression is:

```
((255-fr) * a + fr * b) >> 8   [R162]
```

The only production change is `256u-fr` -> `255u-fr`, with the original-instruction provenance in a local comment. The module dispatch, source tables, phase indexing, FM feedback/modulation state, envelope, sound priorities, scoring, pan and shared DSP mixer retain R161 behavior. All 33 original game-source files are byte-for-byte unchanged.

## Synthetic PCM regression

`tests/test_host_audio.c` now installs two isolated FM descriptors using internal noncopyrighted mathematical sine-wave tables, with carrier phases exactly `32<<16` (fraction zero) and `(32<<16)+0xff00` (fraction 255). One generated output frame per descriptor yields the expected signed stereo PCM samples:

| Phase | R162 expected left | R162 expected right |
|---|---:|---:|
| `0x200000` | 16252 | 16253 |
| `0x20ff00` | 16233 | 16234 |

The new fixture fails when built against the unmodified R161 synthesizer (assertion at the FM sample case), and passes with R162 under both GCC and Clang. It uses generated waveforms and invented descriptors, not the user's ROM.

## Bounded claims / remaining work

- Source-backed fixed-point weight/rounding correction, **not** full hardware audio parity: wave shapes, FM envelope/feedback, compressed samples and interrupt timing still require instruction-level verification.
- No original `AVP.jag` was accessed in this verification environment, so live native-vs-BigPEmu audio comparison is not claimed.
- Earlier Ghidra processor analysis R89–R93 is preserved in `RESEARCH_R159`; no proprietary DSP byte images, Ghidra private project databases or hardware-ROM files are distributed.
- R161's original gameplay, TOM, frontends, allocator and controls remain intact; only `AVP.jag` is needed.
