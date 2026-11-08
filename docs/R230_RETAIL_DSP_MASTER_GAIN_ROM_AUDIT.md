# Alien vs Predator (1994 Atari Jaguar) — R230 original FullSynth DSP master gain

**Parent:** R229. **Scope:** ROM research and documentation only; original production game remains unchanged.

## Original-cartridge chain of evidence

The retail 4,194,304-byte `AVP.jag` SHA-256 is `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Its gzip program at cartridge `$003804` decompresses to **99,072 bytes**, SHA-256 `c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53` (68000 mapped base `$008000`).

At *decompressed* 68000 mapped address **`$00B088`** (stream offset `$3088`), the cartridge contains the original DSP loading descriptor `DSP address=$F1B000; byte count=$0B88`. The following **2,952-byte JERRY DSP image** has SHA-256 `5ce1a2bdb193675eec2c73936e52b05fa82f8156c7a742c4b87200f1d9d1db7e`. No raw image or extracted binary is saved by the audit or distributed.

The original DSP program executes the following common stereo output block, with opcode fields checked directly against the embedded retail bytes:

| DSP PC | Instruction | Effect |
|---|---|---|
| `$F1B100`, `$F1B102` | `SHARQ #15,r9` / `SHARQ #15,r26` | Normalize the full right/left 32-bit voice accumulators **after** voice summing |
| `$F1B104` | `MOVEI #$F1BB40,r2` | Read the master-gain word **address** (DSP `MOVEI` immediate is low-word then high-word) |
| `$F1B10A` | `LOAD (r2),r2` | Fetch master gain from JERRY DSP local RAM as a 32-bit word |
| `$F1B10C`, `$F1B10E` | `IMULT r2,r9` / `IMULT r2,r26` | Multiply each channel's signed **low 16-bit** normalized accumulator by the signed low 16 bits of the same gain |
| `$F1B110`, `$F1B112` | `SHARQ #15,r9` / `SHARQ #15,r26` | Scale the resulting products before output |

**Finding:** `$F1BB40` is the original **common stereo master gain**, not a clock or a music-only control. The stereo gain is applied after both music and effect voices reach the common channel accumulators. The image has only one direct `MOVEI #$F1BB40` literal, at `$F1B104`; this is literal coverage, not a proof that there are no indirect accesses elsewhere.

## Connection to R228–R229 CPU side

R229's 68000 audit establishes six `$7FFF` writes, two zero writes, one `CLR`, two reads and two writes using `$F1BB40`; both fade callbacks preserve the previous VBlank handler. Five original frontend fade-installation call sites pass step `$FA` (250). From the verified endpoints, **132 callback invocations** move the word from `$7FFF` to zero or vice versa. The DSP applies that value through the common output path on its next successful load; precise bus/audio sample timing is not verified here.

## PC-port correspondence — no runtime patch justified

`src/jaguar/host/fullsynth_host.c::avp_jaguar_fullsynth_mix` already sums all voices before the shared signed-16 master gain, shifts by 15 before and after `IMULT`-equivalent multiplication, and uses `F.master_volume` to control both channels. `src/game/music.c` already calls `set_master_volume` during the reconstructed fade callbacks, and `tests/test_music.c` checks the 132-step fade. `tests/test_host_audio.c` already verifies muted and full-scale host playback for a synthetic sound. These establish the mapped host code path, **not** sample-accurate DAC/FullSynth fidelity or exact latency under asynchronous interrupts.

No original cartridge game logic, CPU/GPU/OP renderer, SFX, music sequencer, gameplay physics, audio backend, input, settings, saves or existing tests were modified.

## Reproduction and boundaries

```
python3 tools/audit_retail_dsp_master_gain_r230.py --rom /path/to/legally-owned/AVP.jag
```

The standalone checker validates both ROM and decompressed CPU SHA-256, the embedded DSP image size/hash, instruction opcodes, registers and signed-low-16 gain semantics, CPU fade-word cross references, and a small series of synthetic arithmetic fixtures. It does **not** emit any retail bytes, require a Ghidra database, add game instrumentation, or test actual hardware PCM. Correct sample time, original DSP RAM bus visibility, JERRY interrupt scheduling, custom Jaguar hardware wave ROM, original DAC saturation and real audio/visual A/B remain open. Do not replace the existing renderer or invent a different sound-volume curve from these findings.

**Follow-up:** trace the precise DSP output interrupt/clock-to-DAC loop and the original music change-over ordering against 68000/frontend VBlank scheduling; the separate CROSS GPU/TOM display publication work also remains incomplete.
