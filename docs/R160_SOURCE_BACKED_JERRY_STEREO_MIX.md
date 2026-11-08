# R160 — Original FullSynth stereo mixing execution order

**Source of instruction evidence:** user's public reconstructed Jaguar decompilation,
`Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation`,
`src/frozen_exact/FULSYN_OJ_frozen_exact.s`, plus the independently recovered
RE #15 Ghidra R89–R93 research. The original frozen source is a reconstruction
of the original object-code words, **not the original authors' source spelling**.
The publicly recovered 2,952-byte DSP program is not copied into this release.

## Verified DSP instruction order

The authentic FullSynth DSP program's handler tail at addresses
`F1B2D2/F1B2D4`, `F1B266/F1B268`, `F1B38E/F1B390`, and
`F1B450/F1B452` performs two `IMULT` operations between a signed sample
and the two complementary signed-16 pan coefficients. The operations at
`F1B2D6/F1B2DA`, `F1B26A/F1B26E`, `F1B392/F1B396`, and
`F1B454/F1B458` add those **unshifted** products to the two channel
registers (`r9`/`r26`). These observed pan tails belong to compressed,
compressed+envelope and FM dispatch/continuation paths. Delayed branch
instructions are included in the reasoning; module-local return details and
full runtime reachability remain ongoing work.

After the completed voice-mixing dispatch, the common output route performs:

| DSP PC | Original instruction | Effect |
|---|---|---|
| `F1B100` | `SHARQ #15,r9` | Shift the **total** right-channel sum once |
| `F1B102` | `SHARQ #15,r26` | Shift the **total** left-channel sum once |
| `F1B10C` | `IMULT r2,r9` | Multiply the normalized right channel by master gain |
| `F1B10E` | `IMULT r2,r26` | Multiply the normalized left channel by master gain |
| `F1B110` | `SHARQ #15,r9` | Shift again after master gain |
| `F1B112` | `SHARQ #15,r26` | Shift again after master gain |

At `F1B120/F1B124`, the postmaster outputs are stored to the DSP output
buffer. A distinct original `SAT16S` instruction is **not demonstrated** in
this short path. R160 deliberately retains the host's existing signed-16
saturation and 64-bit intermediate sum; it does **not** claim that the final
DAC/register wrapping matches original hardware sample-for-sample.

## Specific R159 error and R160 correction

R159 rounded `(sample*pan)>>15` separately for each voice, then summed the
already-quantized contributions. This is algebraically different from Jaguar's
`(sum(sample*pan))>>15` when multiple quiet voices are present. R160 retains
full signed products through the shared channel sum and shifts after all
voices have contributed. It applies per-voice host-only gain at the full
product precision only when the gain is other than the default full-scale
setting; the original JERRY has no additional per-voice PC-host gain stage.

A synthetic music test uses two simultaneous compressed-sample notes with a
constant decoded sample value. Previous R159 software rounds its very quiet
left channel lower; R160's left/right output is precisely `(2,3)` for this
fixture. The pre-change code fails the updated sample assertion; corrected
code passes GCC, Clang and both undefined-behavior sanitizers. No commercial
music/waveform data or external ROM is used in the fixture.

## Limits and next priorities

1. The host still uses mathematical approximations of the JERRY wave tables.
   Only `AVP.jag` is required at runtime; no other ROM is loaded or bundled.
2. The original DSP does arithmetic in signed 32-bit registers with a 40-bit
   multiply/accumulate unit; the host retains 64-bit intermediates to avoid
   host undefined overflow and signed-16 saturation at the final output.
   Full wrap/saturation/interrupt/voice-order equivalence is not claimed.
3. Authenticity still requires runtime verification of descriptor mutation,
   effective `REGPAGE`, sample timer interrupts, FM interpolation, and
   complete Original DSP versus BigPEmu PCM comparisons.
4. The same signed pan fields are used for both music and FX, so the shared
   shift correction affects both without modifying original game code.

**Reference:** Atari Jaguar reference, RISC `IMULT`, `ADD`, `SHARQ`, and DSP
`SAT16S`: https://asterick.github.io/jag-docs/reference/risc-instruction-set.html
and https://asterick.github.io/jag-docs/jerry/dsp.html . Original DSP findings
corroborate RE15 Ghidra R89–R93 recovered reports. No Ghidra application or
private Ghidra database was executed in this R160 environment.
