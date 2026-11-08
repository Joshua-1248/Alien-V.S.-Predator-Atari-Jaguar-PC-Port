# R233 — accelerated retail-ROM / PC-port full sweep and next fidelity implementation gates

**Date:** 2026-10-27. **Baseline:** R232. **User mandate:** exact Jaguar cartridge behavior, no diagnostic production patches, no arbitrary changes to working code, preserve credits/licenses, native GCC+Clang Linux packaging, at most four build jobs. **Canonical retail reference:** 4,194,304 bytes, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`, examined locally but **never redistributed**.

## 1. Full sweep result and new proof

Run all `tools/audit_retail*.py` on the same authenticated original ROM; **25 pass, 0 fail**. Previous 24 source-instruction checkers remain valid. The R233 new checker `tools/audit_retail_jerry_timer1_music_r233.py` validates the full original timer register *data path*, not merely the interrupt-vector names:

- 68000 reset calls at `$0097A0` and `$0098BA` each clear Timer 1 prescaler (`$F10000`) and divider (`$F10002`).
- Original score/tempo descriptor words at offsets **`+$10`** and **`+$12`** are written, respectively, to **divider `$F10002`** at CPU `$009A26` and **prescaler `$F10000`** at `$009A2E`. Original CPU score-bookkeeping call at `$009A20` is included as a complete instruction check.
- Original FullSynth DSP loads addresses `$F10000` and `$F10002` at `$F1B5D2/$F1B5D8`, writes a 16-bit value of 1 to both from `$F1B5DE..E2`; its distinct Timer 1 handler begins at `$F1B5EE` and uses sequencer-state pointers to `$F1B98C`, `$F1BA84`, `$F1BAC4`, `$F1BAFC` and `$F1BB3C`.
- All raw 68000 longword references to the two timer addresses are enumerated. One apparent `$F10000` occurrence is inside a data table at `$853A`, not an executable write; it must not be treated as a fourth Timer1 writer.
- Existing host (`src/jaguar/host/fullsynth_host.c`) decodes the same descriptor offsets into `div` and `pre` and computes a timer period with `(pre+1)*(div+1)`. Host uses the nominal 20,774 Hz audio rate and 26,590,906 Hz processor clock. Descriptor **field orientation** is verified; sample-exact PIT interrupt phase, audio buffer synchronization and exact music notes are **not**.

Hardware reference: Atari Jaguar JERRY has two cascaded 16-bit Timer 1 dividers, each divides by N+1, and music may schedule against its interrupts. `https://asterick.github.io/jag-docs/jerry/audio.html`. Do not confuse 68000 initialization, the DSP timer-rearm block, interrupt occurrence, and host generation of individual samples.

## 2. Highest-impact source-to-original gaps: stop repeating the same audits

The sweep searched the currently shipped **production implementation**, not only historical addenda. Concrete unresolved substitutions identified:

| Priority | File and source evidence | Exact original parity gate | Status |
|---|---|---|---|
| 1. Character selection / title / high-score frontend visual transitions | `src/platform/pc/pc_runtime.c` defines `PC_SELECT_FADE_VBLS=40`, `PC_SELECT_BLACK_VBLS=188`; other title ↔ fame paths use video-estimated 40-VBlank fades and an extra 10-VBlank black hold. Source comments explicitly identify **video-derived approximations**. | Recover original timed caller instructions, GPU phase buffers, `$7FA8` VBlank/list handshake and TOM screen-list samples; compare a same-input retail recording or hardware reference. Do **not** substitute 132 audio callbacks for visual ticks. | **Not 1:1 verified; needs replacement** |
| 2. CROSS GPU/OP visible-frame output | Original 16 GPU runtime blend stages and `$7FA4/$7FA8` list handoff known from R221–R228; existing host effect is an approximation without bus/OP scanout proof. | Couple the original 68000 VBlank/list path to RISC and Blitter memory transactions and TOM active lines; produce original-reference frame hashes before editing running renderer. | **Instruction path verified, hardware display not** |
| 3. FullSynth hardware waveforms and precise PCM | `src/jaguar/host/fullsynth_host.c` explicitly initializes approximate wave tables and sets `host_wave_rom_exact=0`. Authentic external wave-ROM input is supported but the default is not exact. | Verify original Jaguar internal waveform-table bytes with a legally supplied authentic dump / validated primary implementation, then run PCM A/B including IRQ phase and serial output. | **Non-exact default; no commercial wave data bundled** |
| 4. Timer/serial DSP scheduling | Timer divider field mapping now proven; R231/R232 confirm DSP serial output and two ISR vectors. `fullsynth_host.c` drives music tempo from host output samples via accumulator. | Execute or instrument a **separate offline original-DSP emulator** with authentic timer register reset/rearm/interrupt latency, original tone/score data and serial FIFO. Do not ship diagnostics inside the PC game. | **Clock relation known, actual IRQ phase not** |
| 5. High-resolution pause architecture guard | `tools/audit_architecture.py` rejects host's `PSVO` event literal in `pc_video_x11.c`. Host uses that event to pre-copy displayed high-resolution pixels before Jaguar-owned pause SaveOvers. | Agree on ownership preserving that pre-copy and tests, then migrate if the original proves it belongs elsewhere. | **Known R231–R233 guard failure; not a gameplay regression** |
| 6. Campaign-wide 1:1 validation | Existing synthetic renderer and gameplay regressions are extensive; they do not equate to a hardware/matched-ROM recording of Alien/Marine/Predator routes. | Side-by-side route capture and machine-state comparisons for death/game-over, corpses, menu/high score, HUD, all terminal/door transitions, cheats, saves and audio. | **Not complete; needs A/B reference** |

**Important:** Audio fades are separately known from original CPU callbacks (R229: 132 steps at ±250 between endpoints). The host's 40-VBlank visual fades are NOT proved equivalent and must not be silently changed. Prioritize one end-to-end original frontend scene before more isolated opcode audits.

## 3. Validation and preservation policy

- Original-ROM authenticated audit matrix: **25/25 pass** on this revision (24 carried forward + original JERRY Timer1/musical-descriptor proof).
- Public source release exclusion audit: pass. Original ROM, copyrighted DSP payload and waveform ROM are not in the deliverables.
- Architecture rule `pause OP fourcc PSVO`: pre-existing fail, retained and documented. Never weaken a check to claim a passing suite.
- Regression suites and fresh binaries: refer to `R233_VERIFICATION.md` for **observed** outcomes; do not conflate externally timed frontend boot with natural exit.
- Production source `src/`, `include/`, `tests/`, `cmake/` and CMake config **unchanged** from R232; new audit and records are research-only.

## 4. Fastest honest path to finishing

Stop making one-off ROM checker releases as though they close visual parity. The blocking dependency for the original CROSS effect is a **coupled** CPU interrupt → GPU/Blitter row completion → OP list-buffer swap → active scanline acquisition trace, using instruction-accurate original programs. Group the 16 phases and all 144 rows in a single offline integration run and compare original video output, then implement only proven differences. In parallel, create verified DSP Timer1/I²S sample event trace and compare wave-ROM-backed PCM output. The present artifact does **not** claim these remain unimplemented because of a compiler failure; they lack the original hardware reference and execution timing evidence needed for non-approximate replacement.
