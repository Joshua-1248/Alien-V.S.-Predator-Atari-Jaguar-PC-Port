# R165 — Original JERRY FM envelope end comparison: wrapped CMP N flag

**Date:** October 22, 2026  
**Playable baseline:** R164; targeted host-side audio arithmetic correction only.

## Authoritative instruction evidence

The user's public, frozen-exact 2,952-byte FullSynth DSP reconstruction, `src/frozen_exact/FULSYN_OJ_frozen_exact.s` in `Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-Decompilation`, supplies the original instruction words. Earlier RE #15 R89–R93 Ghidra research independently establishes the FullSynth DSP code/data map, voice-descriptor stride, register indexing and module dispatch.

In the FM handler beginning at DSP `$F1B2DC`, after the envelope phase is advanced, the original sequence includes:

| Original DSP PC | Recovered instruction | Meaning for this fix |
|---|---|---|
| `$F1B2F6` | `ADD r23,r22` | Advance the envelope phase in a 32-bit register. |
| `$F1B330` | `LOAD (r15+11),r24` | Load the envelope limit (post-dispatch physical descriptor word 12). |
| `$F1B332` | `CMP r22,r24` | Calculate flags for `r24 - r22` without writing the subtraction. |
| `$F1B334` | `LOAD (r15+12),r25` | Load another envelope parameter; does **not** modify CMP flags. |
| `$F1B336` | `JR cc20` | Branch on the documented **N flag clear** condition. |

Atari's Jaguar RISC reference specifies the negative flag as bit 31 of a 32-bit arithmetic result; `CMP` performs subtraction without storing the result, and `JR` tests the flags. See https://asterick.github.io/jag-docs/reference/risc-instruction-set.html .

The previous host used `if (ep > m->p[12])`. This is an *unsigned relational comparison*, which is not the same as the original DSP's `N` bit on a wrapped 32-bit subtraction. The correction is:

```c
if (((m->p[12] - ep) & 0x80000000u) != 0u) {
    /* existing original envelope end/loop action unchanged */
}
```

The subtraction is in `u32`: defined C unsigned wraparound exactly models a Jaguar 32-bit register. It does **not** replace the hardware condition with a signed relational comparison, which would also be wrong in wrap cases. No change is made to the branch's existing release/loop actions, voice assignment, descriptor fields or clocking.

## Synthetic regression (no commercial data)

`tests/test_host_audio.c` now compares three otherwise identical generated-sine FM descriptors whose envelope-end words differ. All start with envelope phase `$00010000` and do not loop:

| End register | Wrapped `end − phase` | DSP N | Expected |
|---|---|---|---|
| `$00200000` | `$001F0000` | 0 | Sample sounds |
| `$FFFFFFFF` | `$FFFEFFFF` | 1 | Voice retires; stereo output zero |
| `$80000000` | `$7FFF0000` | 0 | Same output as normal case |

The **unchanged R164 synthesizer fails** the new `0xffffffff` test, while R165 passes GCC and Clang. This is an instruction-semantics edge case; it does not establish that any ordinary retail score reaches such envelope endpoints.

## Scope and limitations

Only `src/jaguar/host/fullsynth_host.c` and its existing synthetic regression `tests/test_host_audio.c` are changed. All 33 original gameplay files, music score and patch data, waveform approximations, graphics, controls, save handling and title/character-selection presentation stay unchanged. `AVP.jag` remains the **sole required ROM**. No additional proprietary waveform ROM, commercial game assets, original DSP bytes, or private Ghidra project is distributed. Neither real Jaguar audio output equivalence nor the actual timbre of the character-selection song is asserted.

Earlier Ghidra R89–R93 records are retained as provenance. This release does not claim a fresh local Ghidra execution; the public frozen-exact assembly and hardware reference were used directly.
