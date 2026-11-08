# R163 — Original JERRY FullSynth FM feedback low-half multiplication

## Original-instruction evidence

The recovered original FullSynth program, preserved in the user's public
`Alien-V.S.-Predator-Atari-Jaguar-Decompilation` repository as
`src/frozen_exact/FULSYN_OJ_frozen_exact.s`, includes the FM handler at DSP
`$F1B2DC`, also identified by the prior verified RE #15 Ghidra R93 analysis.

This handler's feedback/modulation math performs:

- `$F1B370` `LOAD (r15+15),r22`: physical descriptor word **16**, accounting
  for the original 4-byte dispatch delay-slot advance to `r15`.
- `$F1B376` `IMULT r16,r22`: multiply the **low signed 16 bits** of the
  modulation/envelope value and descriptor word 16.
- `$F1B37A` `SHARQ #15,r22`; `$F1B37E` `ADD r22,r8`: fold the scaled
  contribution into the feedback index that starts from physical word 9.
- `$F1B382` `IMULT r8,r4`; `$F1B386` `SHARQ #7,r4`: multiply the low signed
  halves of the modulating waveform sample and feedback index; store the
  resulting offset back to physical descriptor word 5 at `$F1B388`.
- `$F1B37C` `IMULT r16,r13` and `$F1B38A` `SHARQ #15,r13`: apply the same
  low-half arithmetic to the audio modulation gain before pan mixing.

`IMULT` is a signed **16×16 → 32** operation regardless of the upper half of
its source or destination register. This is specified in the Atari Jaguar
RISC reference: https://asterick.github.io/jag-docs/reference/risc-instruction-set.html .
The earlier RE #15 R89/R93 Ghidra work supplies the original register/dataflow
crosswalk. R163 did not execute a new Ghidra instance.

## R162 mismatch and narrow R163 correction

In R162, `synth_sample_fmenv()` used 64-bit C multiplication involving the
*entire* descriptor `p[16]`, `p[9] + depth`, and extended modulation/envelope
values. For these original `IMULT` sites this gives a different answer when
an operand's upper 16 bits are nonzero. R163 applies an already-present,
well-defined `dsp_low16_signed()` conversion at the three identified
low-half multiplication sites, preserving the existing shift counts, phase
updates, module dispatch, voices, and PCM output stage. The helper was moved
earlier within the same source file so the FM code can use it; it was not
reimplemented or changed.

No alteration was made to the mathematical internal JERRY waveform shapes,
other original instruments, music score data, timer rate, frontends, or
original game logic.

## Independently checked synthetic regression

`tests/test_host_audio.c` defines four noncommercial FM descriptors using
mathematical carrier/modulator waveforms and identical **low 16-bit** feedback
parameters. Their `p[16]` and `p[9]` upper words differ by `0x10000`.
All four must render identical 64-frame stereo PCM buffers because `IMULT`
ignores those upper word bits. This test fails against the unchanged R162
FullSynth implementation and passes against R163 under both GCC and Clang.
Existing sine/carrier and compressed-sample tests continue to run without
original game audio assets. The arithmetic is also checked using GCC and
Clang undefined-behavior sanitizers.

## Remaining fidelity limits

This verifies only the identified register-width operations, not the entire
40-bit DSP MAC pipeline, exact FM envelope transitions, table contents,
interrupt clocking, register-page switches, hardware wave-ROM sample identity,
or bit-for-bit BigPEmu audio parity. Original `AVP.jag` remains the only
required input ROM; no external firmware or copyrighted sample dumps are
loaded or shipped. Prior RE #15 Ghidra reports remain packaged as preserved
research, not proof of a new live Ghidra run.
