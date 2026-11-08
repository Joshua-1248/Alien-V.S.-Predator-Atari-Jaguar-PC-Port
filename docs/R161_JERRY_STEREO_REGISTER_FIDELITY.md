# RE #18 R161 — Original JERRY DSP stereo-register semantics

## Evidence and source

- Original retail FullSynth instruction words reconstructed in the user's public
  `Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation` repository,
  `src/frozen_exact/FULSYN_OJ_frozen_exact.s`. This is a source reconstruction
  of **object code**, not original author-written source spelling.
- Earlier original-binary Ghidra R89–R93 reverse-engineering reports,
  recovered from RE #15 and preserved under `RESEARCH_R159`.
- Jaguar RISC instruction reference: https://asterick.github.io/jag-docs/reference/risc-instruction-set.html
- Atari JERRY DSP architecture reference: https://asterick.github.io/jag-docs/jerry/dsp.html

## Original instruction facts

The per-module stereo pan `IMULT` paths read the **signed bottom 16 bits** of
both operands, writing a 32-bit signed product. At the module tails, `ADD`
accumulates those results into **32-bit** GPRs `r9` and `r26`, discarding any
carry beyond bit 31. Examples of original code addresses:

| Original PC(s) | Operation |
|---|---|
| `F1B266/F1B268`, `F1B2D2/F1B2D4`, `F1B38E/F1B390` | Signed low-half `IMULT` with per-voice pan coefficients |
| `F1B26A/F1B26E`, `F1B2D6/F1B2DA`, `F1B392/F1B396` | 32-bit `ADD` into right `r9` and left `r26` accumulators |
| `F1B100/F1B102` | `SHARQ #15` on the **wrapped 32-bit channel registers** |
| `F1B10C/F1B10E` | `IMULT` of **signed low 16 bits** of normalized channel and master gain |
| `F1B110/F1B112` | `SHARQ #15` of those master-multiplied 32-bit results |
| `F1B120/F1B124` | Store completed channel samples into original DSP ring buffer |

These are processor instruction semantics, not a blanket assertion of
cycle-accurate Ghidra simulation. The actual DSP 40-bit multiply/accumulate
unit applies to `IMULTN`/`IMACN`/`RESMAC`, not the independent `ADD rXX,r9`
channel accumulation sequence. R156 broadened the host stereo accumulators to
64-bit to avoid undefined C overflow; R160 maintained that choice. Avoiding
undefined behavior was correct, but an **unbounded sum** is not what the
original JERRY register does.

## R161 fix

`src/jaguar/host/fullsynth_host.c` retains R160's critical ordering: sum
unshifted voice pan products, shift combined channels once, then multiply by
master gain and shift again. R161 also:

1. Uses explicitly signed 16-bit operands for each per-voice `IMULT` product.
2. Accumulates channel products through C `uint32_t` additions, preserving
   hardware 32-bit wrap without signed-overflow UB.
3. Converts the final register bit pattern to signed 32-bit using a widened
   subtraction rather than implementation-defined casts.
4. Applies `SHARQ #15` to the resulting signed 32-bit channel registers.
5. Uses the signed **low 16 bits** at the master `IMULT`, instead of
   multiplying the entire shifted accumulator by the volume in 64-bit.
6. Preserves PC-only gain controls, default gain unity, and the existing final
   output signed-16 clamp. Exact DAC/peripheral handling is a separate task.

## Before/after reproduction

`tests/test_host_audio.c` includes a synthetic 16-voice case that deliberately
cross the signed 16-bit range **after** stereo normalization and, in two
cases, also overflow the 32-bit channel ADD register. Their raw samples
are not from commercial ROM assets. Expected output is left/right:

| Synthesized sample | Expected signed L/R after Jaguar low-half `IMULT` |
|---|---|
| Positive, synthetic `0x3c` | `(-8180,-8176)` |
| Negative, synthetic `0xbc` | `(8162,8159)` |
| Positive, synthetic `0x64` (also wraps 32-bit ADD) | `(28277,28287)` |
| Negative, synthetic `0xe4` (also wraps 32-bit ADD) | `(-28295,-28304)` |

The R160 mixer **fails** the revised regression because its old unbounded
64-bit sum saturates at `+32767` or `-32768` for these inputs. R161 passes under GCC and Clang.
The existing very quiet two-note rounding fixture still passes.

## Open technical questions and limits

- JERRY's hardware waveform ROM data is not present in `AVP.jag`; the host
  continues to generate internal approximate waveforms without extra ROMs.
- A complete instruction-by-instruction FM waveform and compressed-sample
  comparison remains necessary; R161 does not alter FM envelope or table math.
- The original DSP's register banks, interrupt cycles, voice dispatch runtime
  paths and original device output format have not been dynamically proven.
- The last host signed-16 PCM clamp is preserved, not newly certified
  sample-identical to JERRY's DAC.
- No private DSP binary, copyrighted ROM/audio, Ghidra database, or emulator
  code is copied or bundled.
