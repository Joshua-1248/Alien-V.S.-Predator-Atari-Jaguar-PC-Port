# R164 — Original FullSynth JERRY FM-envelope MAC operand widths

## Scope and provenance

RE #18 R164 extends R163 using the original frozen-exact recovered Jaguar
FullSynth DSP instruction words from the user's preservation repository,
`Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation`, at
`src/frozen_exact/FULSYN_OJ_frozen_exact.s`, together with the previously
executed RE #15 R89–R93 Ghidra findings and Atari's documented Jaguar RISC
instruction semantics:
https://asterick.github.io/jag-docs/reference/risc-instruction-set.html

The original FM handler's 32-bit envelope lookup and interpolate sequence:

| DSP PC | Original instruction | Relevant operation |
|---|---|---|
| `$F1B324` | `LOAD (r14+r2),r3` | Load full 32-bit envelope table cell |
| `$F1B32C` | `LOAD (r14+r2),r11` | Load the neighboring full 32-bit cell |
| `$F1B36A` | `IMULTN r3,r10` | Signed **low 16-bit** multiplication, no register write-back |
| `$F1B36C` | `IMACN r11,r12` | Second signed **low 16-bit** multiplication accumulated with the first |
| `$F1B36E` | `RESMAC r13` | Read the resulting MAC register |
| `$F1B372` | `SHARQ #8,r13` | Arithmetic shift of the combined value |

This is a *bounded instruction/dataflow observation*. The original DSP
interpolator combines 8-bit complement and fraction weights; the R162
correction to `255-frac` is retained. It is not evidence that all FM
pipeline state, interrupt timing, or the original hardware waveform contents
have been reconstructed exactly.

## R163 mismatch

`src/jaguar/host/fullsynth_host.c::synth_sample_fmenv()` reads the envelope
cells with `dsp_table_long()` into `s32` variables. R163 passed those full
32-bit C values to the two interpolation multiplications. JERRY's `IMULTN`
and `IMACN` instead only multiply the signed low 16 bits of those registers.
R164 uses the existing `dsp_low16_signed()` helper on both envelope cells,
keeping the original coefficient, sum, and shift structure unchanged.

## Reproduced old defect and R164 verification

`tests/test_host_audio.c` adds two synthetic FM descriptors using separate
original-style DSP envelope-table addresses. One contains 32-bit cell values
`0x00007fff` and the other `0x00017fff`. Their lower 16 bits are identical.
With the original Jaguar `IMULTN`/`IMACN` semantics the rendered 64-frame
stereo buffers **must be identical**. With the updated test but the unmodified
R163 mixer, the PCM comparison fails. After the narrow R164 change, GCC and
Clang each pass this comparison. The fixture uses only generated waveform
approximations and made-up envelope data; it has no copied game assets or
hardware-ROM bytes.

## Preserved behavior and known open issues

Only the hosted FullSynth envelope interpolation and its existing synthetic
audio test have been modified. None of the 33 files in `src/game/`, audio
patches/score data, oscillator tables, music timing, frontend transitions,
controls or saves has changed. Master mixing remains on the R161 register-
width and post-voice shift path. `AVP.jag` alone remains the required ROM.

The optional absence of high halves in ordinary retail envelope cells could
make the audible impact negligible in existing tracks; the source-backed
operand-width correction does **not** establish improved real-game timbre or
BigPEmu PCM parity. Original JERRY waveforms, full 40-bit MAC and register
bank semantics, interrupt/DAC schedule and 1:1 music comparison remain open.
Prior RE #15 Ghidra results are references; no new Ghidra invocation or
private-ROM analysis is claimed for R164.
