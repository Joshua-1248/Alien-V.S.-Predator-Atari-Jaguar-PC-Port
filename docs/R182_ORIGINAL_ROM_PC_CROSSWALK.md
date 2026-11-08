# R182 — Original Jaguar ROM to PC-port fidelity crosswalk

## Source-of-truth hierarchy

1. **Retail `AVP.jag` actual bytes**: authoritative for original code, data, resources. On this pass ROM SHA-256 is `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. `RDB.TEXT` and `RDB.DTA` gzip streams at offsets `0x3804` and `0xE840` independently decompress and match the known frozen binary hashes (99072 and 60248 bytes).
2. **Original assembly/Ghidra instruction mapping**: needed where reconstructed C is ambiguous. Retained frozen 68000 images and prior Ghidra reports are references. **This pass does not claim to have disassembled the entire gameplay ROM instruction-by-instruction.**
3. **Readable 68000 C reconstruction**, particularly `MAZE.S` → `src/game/maze.c`, `MAZESCRN.S` → `src/game/mazescrn.c`, `LEVELS.S` → `src/game/levels.c`, `MJP.S` → `src/game/mjp.c`. Useful for CPU order and semantics; not sufficient to certify GPU/OP raster timing.
4. **PC host runtime**, `src/platform/pc/pc_main.c`, `src/platform/pc/pc_runtime.c`, and `src/platform/pc/pc_video_x11.c` plus Jaguar GPU host renderer. Every host-only workaround should be checked against the original order before changing gameplay.

## Crosswalk: original versus R181 versus R182

| Original subsystem | Original reference sequence | R181 PC implementation | R182 result and limits |
| --- | --- | --- | --- |
| Duct level transition | `new_level` crossing duct boundary → duct SFX → `FadeDown()` → `LeaveLevel()` → `LoadLevel()` → `load_complete` → `ResetMaze()` → wait only while `!faded` → `fade_level=0x80`, `fade_lim=0`, `fade_rate=-2` | Performed full `pc_finish_visible_gameplay_fade()` **before** `LeaveLevel` and level load; therefore faded AND loaded sequentially rather than overlapping | Fade epoch and outgoing image captured before `LeaveLevel`; waits after `ResetMaze`; elapsed loading time counted toward original 64-VBlank fade. Exact TOM/OP output and real hardware loading time remain unverified. |
| Terminal/computer entry | terminal request → stop looping SFX → `FadeDown()` → wait until `faded` → `Computer()` → exit SFX → fade-in | Waited to black before `Computer()` | **Unchanged ordering**. No need to conflate with duct path. |
| Fade counter | `FadeDown`: level 0, limit `0x80`, rate `+2`; `MazeList`: increment once each VBlank until limit | Source-preserved `FadeDown` and `MazeList`, monotonic clock replay in host | **Unchanged CPU fade arithmetic**. Approximate host 60Hz projection and frame snapshot are not full hardware reproduction. |
| Door persistence | `LeaveLevel()` executes `SaveDoors`, actor save, then `cur_level=new_level`; `LoadLevel()` restores original per-level objects/state | R178 corrected new-campaign cached map reset; ordinary interlevel doors persist | **Unchanged** by R182. Independent test passes, whole-playthrough comparison still needed. |
| Corpse persistence | `ram_save` retains valid dead placements; restore uses dead template/flags | R171 adjusted save/filter/rebuild | **Unchanged** by R182. Original-C crosswalk and tests exist, not a fresh instruction trace. |
| Title, character selection | MJP object/resource/tick scripts include original authored tracks and timed phases | Renderer mixes original sprite records with PC-host interpolations | **Open**: original GPU scaling/raster, zoom and exact fades need frame comparison. Do not invent durations. |
| Game Over, Hall of Fame | MJP `Do_Lose`, `Do_Fame`, text and score operations | PC renderer uses original text and source-derived CPU state, but some fade/row timing remains recording-calibrated | **Open**: full OP/GPU raster equivalence and high score handoff still unproved. |
| JERRY audio | JERRY DSP, original cartridge SFX and score descriptors | `fullsynth_host.c` approximates several hardware-dependent wave operations | **Open**: actual hardware waveform and mixed output remain unverified; do not boost global gain to mask it. |

## Exact duct CPU sequencing (source-reconstructed)

```
Original PlayAvP:
    if (entering_or_leaving_duct) FadeDown();
    LeaveLevel();
    LoadLevel();
load_complete:
    ResetMaze();
    if (in_fade) {
        in_fade = 0;
        while (!faded) display_wait_tick();
        fade_level = 0x80; fade_lim = 0; fade_rate = -2;
    }
```

R182 PC arrangement mirrors this ordering while using a monotonic time offset for the synchronous host loader. In particular this revision does **not** replace the 64 original increments with an invented 8- or 12-frame fade. That would be unsupported without additional original evidence.

## Next precision work to reach 1:1 fidelity

- Produce controlled ROM/BigPEmu and PC captures of an actual named duct transition in both directions with common species, level, entry cell, resolution and nominal 60-Hz capture; compare first fade frame, black endpoint and first destination frame, separating original Jaguar loading overlap from CPU fade state.
- Reconstruct/disassemble the original transition's 68000 handler branch and the GPU/OP fade modifier implementation where the readable C is insufficient; verify screenshot-based fade image and CRY RMW operations at each phase.
- Repeat per-feature ROM → recovered assembly → C → PC-host crosswalk for menu/font resource addressing, character selection and ending scenes, door initialization/persistence, enemy collision/AI, and FullSynth voice arithmetic. Fix only a demonstrated deviation, with matched regression and compiler builds, never change already-correct gameplay opportunistically.

**Limits:** Byte-identical decompressed ROM streams prove the correct source material, not the correctness of every reconstructed function. Passing focused tests proves these paths behave as specified by the tests, not global 1:1 fidelity. The user’s supplied ROM is a local verification input, never a redistributable output.
