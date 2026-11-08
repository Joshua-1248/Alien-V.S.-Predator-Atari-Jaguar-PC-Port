# R229 — Retail DSP-RAM fade-control word and 68000 callback arithmetic

**Parent:** R228. **Scope:** original cartridge research/checker and cumulative project history only. Production gameplay, rendering, audio, input, saves, tests and existing audit tools are not modified.

## Primary evidence

The user's original 4,194,304-byte `AVP.jag` is SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Its embedded compressed 68000 program at cartridge offset `$3804` expands to 99,072 bytes; SHA-256 `c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53`. The new checker authenticates both before doing any analysis.

The absolute CPU operand `$00F1BB40` is physically **JERRY DSP local RAM**, not an autonomous timer register. All **13** aligned occurrences of this address literal in the original decompressed 68000 executable have been matched to full original instructions:

| Type | Verified 68000 instruction starts | Count |
|---|---|---:|
| `MOVE.L #$00007FFF,($F1BB40)` | `$0086F0`, `$008872`, `$00985E`, `$00CECE`, `$00CF20`, `$00F1C8` | 6 |
| `MOVE.L #0,($F1BB40)` | `$00CE8A`, `$01C1D6` | 2 |
| `CLR.L ($F1BB40)` | `$00CEEA` | 1 |
| `MOVE.L ($F1BB40),D0` | `$009ACA`, `$009AF6` | 2 |
| `MOVE.L D0,($F1BB40)` | `$009AEC`, `$009B1E` | 2 |

This is **full literal-occurrence coverage** of this address in the ordinary 68000 executable, not proof that the DSP program has no additional accesses, nor proof against indirect 68000 addressing. Earlier R228 documentation correctly placed the address in DSP RAM but did not yet identify the original fade endpoints or complete CPU literal inventory.

## The two fade directions

The two original timer callback installers and the corresponding original callback bodies (already identified in R228) now have a source-backed numeric interpretation:

* `$009A5C` installs callback `$009ACA`. It subtracts its saved step `$2EF44` from the RAM word (`SUB.L`) and branches while the **32-bit** result is nonnegative; when negative it stores **zero**, clears the countdown and restores the previous VBlank callback. The original direct calls at `$01CE16`, `$01EF4E`, `$01F88A` all immediately follow `MOVE.L #$FA,D0`.
* `$009A80` installs callback `$009AF6`. It adds its saved step, then tests only the **low 16-bit word** (`TST.W`) and branches while the word is nonnegative. When its sign bit becomes set, it snaps the result to `$7FFF`, clears the countdown and restores the prior callback. The two original direct calls at `$01C0B0`, `$01C1F4` both immediately follow `MOVE.L #$FA,D0`.
* Both callbacks invoke the saved previous VBlank callback from the shared tail `$009B24` even during the fade, as verified in R228. Thus the fade and Object Processor list publication are chained, not mutually exclusive.
* The frontend's mode-3 path explicitly writes **zero** to `$F1BB40` at `$01C1D6` before issuing the fade-up call at `$01C1F4`.

For a starting word of `$7FFF` and original step `$FA` (250), fade-down performs **132 callback invocations** before clamping to 0. From zero, fade-up performs **132 callback invocations** before clamping to `$7FFF`. At step 131, the values are 17 (down) and 32,750 (up), respectively; step 132 causes the original signed-condition branches to take the saturation paths. These are **callback counts** under stated initial conditions, not independently established elapsed seconds, NTSC/PAL frame counts or an audio playback envelope on Jaguar hardware.

## Interpretation and limits

The evidence establishes a **software-controlled DSP-RAM fade level with explicit full/zero endpoints**, used in frontend transitions and initialization. The pair of opposite-direction update routines and the `#250` caller arguments support identifying them as fade control. R228's cautions remain in force: the full DSP-side producer and consumers, whether the word controls global mixer level versus a more specific audio path, the waveform heard on real hardware, interrupt-to-audio propagation, TOM scanline visibility and the complete CROSS GPU transition are **not yet proven**. Do not guess or apply a replacement volume curve or alter the existing PC backend from these static facts alone.

## Reproduction

```
python3 tools/audit_retail_frontend_dsp_fade_word_r229.py --rom /path/to/AVP.jag
```

This independent offline checker authenticates the original ROM and decompressed program, checks every aligned absolute literal reference, all five original direct fade calls and the exact subtraction/addition, conditional tests and snap-to-endpoint instructions. It calculates the conditional endpoint steps from those instructions. It does not modify or emit proprietary ROM content, instrumentation, or production code.

**Next:** Recover and execute the relevant DSP-side accesses to `$F1BB40`, determine exact mixer/level interpretation, correlate the original fade callback scheduling with music start/stop and original frontend VBlank publication, and only then address PC audio/render fidelity with verified values.
