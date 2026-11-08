# Alien vs Predator (1994 Atari Jaguar) — PC port R181

**Baseline:** R180 (RE #19, October 22, 2026). **Scope:** vent/duct and computer fade presentation pacing.

## Retail-ROM-derived rate versus the R179 host slowdown

The user's original canonical 4 MiB `AVP.jag` (SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`) was verified and its 68000 executable gzip at cartridge offset `$3804` decompressed successfully to 99,072 bytes. The recovered `MAZE.S`/`MAZESCRN.S` C representation in `src/game/maze.c::FadeDown()` and `src/game/mazescrn.c::MazeList()` expresses the original progression: `fade_level=0`, `fade_lim=$80`, `fade_rate=+2`, one advance per Jaguar display VBlank, total 64 increments. This corroborates the fade *counter*; it does not prove original live scene visibility for all 64 frames or original level-loading time.

R179's hosted screenshot transition called `VSync()` for every step. The PC implementation of `VSync()` repaints a full-size high-resolution fade frame and **then unconditionally sleeps an extra 16.7 ms**. On costly render/scale/X11 configurations the displayed vent fade therefore took *64 times (render time + 16.7 ms)* rather than 64 NTSC display periods. This was an unintended host-only slowdown, not original Atari Jaguar behavior. The same unnecessary waits affected the computer-entry fade.

## Narrow PC-only correction

A common host transition routine `pc_finish_visible_gameplay_fade` now uses the monotonic nanosecond clock and the original 60-Hz Jaguar VBlank duration to govern the fade. At each presentation opportunity, it advances `MazeList()` through the VBlank steps that have actually elapsed, presents only the latest due framebuffer, and sleeps **only until the next VBlank deadline**. If an expensive frame overruns one or more VBlanks, the missed presentation frames are skipped while the original fade counter still receives every `+2` increment. It does not slow the animation by adding a per-frame sleep after the render work. Original intensity math, final black endpoint, audio, levels, terminal behavior, door persistence, and render interpolation are preserved.

No fixed accelerated fade rate or arbitrary 8/12/16-frame shortcut has been substituted for the Jaguar's 64-step counter. This corrects PC wall-clock pacing rather than claiming new GPU/OP fidelity. **It does not claim that the entire 64-tick outgoing-room hold introduced by R179 has been proven 1:1 to the original hardware load overlap.** That aspect of the original's immediate `LeaveLevel`/`LoadLevel` ordering remains a separate source-informed fidelity issue requiring live vent footage and loader timing comparison.

A header `include/pc/pc_fade_clock.h` centralizes the original VBlank count and capped wall-time-to-tick calculation; a native `tests/test_pc_fade_clock.c` checks tick boundaries, saturation and catch-up after an expensive frame. There are no diagnostic or instrumentation patches in the shipped game.

## Verification

- Strict GCC/Clang native Linux Release builds: **PASS**.
- GCC/Clang focused `test_pc_fade_clock`, `test_campaign_end`, `test_levels_doors`, `test_pc_pause_world_snapshot`, `test_frontend_fade`, `test_mjp`: **PASS**.
- GCC/Clang undefined-behavior sanitizer for the fade clock regression: **PASS**.
- Original `src/game` files: all 33 **BYTE-IDENTICAL** to R180.
- Complete long-running gameplay regression suite and live vent/terminal comparison in BigPEmu: **NOT RUN**. The dynamic fade's latency under the user's actual display resolution must still be playtested.

## Release policy

Current complete source, GCC/Clang x86-64 Linux executables (0755), R181 release/verification notes, cumulative R001–R181 addendum, preserved research, licenses and credits. No commercial ROM, extracted artwork, supplemental wave ROM, private Ghidra databases, save files or old release ZIPs. Supply the existing `AVP.jag` privately.

# R181 verification — October 22, 2026

| Check | Result |
| --- | --- |
| GCC strict Release build | PASS |
| Clang strict Release build | PASS |
| GCC/Clang VBlank fade clock regression | PASS |
| GCC/Clang `test_campaign_end` | PASS |
| GCC/Clang `test_levels_doors` | PASS |
| GCC/Clang `test_pc_pause_world_snapshot` | PASS |
| GCC/Clang `test_frontend_fade` | PASS |
| GCC/Clang `test_mjp` | PASS |
| GCC/Clang fade-clock UBSan | PASS |
| Original source under `src/game` | 33 files byte-identical to R180 |
| ROM SHA-256 and executable gzip | VERIFIED (canonical original image) |
| Full long-running game regression | NOT RUN |
| Live vent/terminal actual-game visual A/B | NOT RUN |
| Exact OP/GPU loading overlap equivalence | NOT PROVEN |

The correction prevents the PC 16.7ms per-frame sleep from accumulating on top of expensive software presentation. It preserves the 64-step source fade counter, but is not a claim of final original Jaguar timing parity in every loading condition.
