# Alien vs Predator (1994 Atari Jaguar) — PC port R176

**Baseline:** R175, October 22, 2026. **Scope:** Game Over / Hall-of-Fame ongoing presentation fidelity.

## Original retail recording analysis

The user's original 60-fps BigPEmu recording `2026-10-21 15-32-16.mp4` was analyzed at individual frame positions rather than relying on a guessed easing profile. The bright orange third ranked entry's top edge moves from approximately screen Y=974 in frame 2810 to Y=446 in frame 2857, with successive frames typically differing by ~10–12 captured-screen pixels and without slowing over this interval. The final motion stops sharply on frame 2857. Rank rows in the recorded scene are separated by ~90 captured pixels, corresponding to 24 native Jaguar pixels; image scale is thus approximately 3.75. Motion is (974-446)/47/3.75 ≈ 3 native picture pixels per video frame.

## Narrow implementation

In the PC-only renderer `src/jaguar/avp_gpu/frontend_renderer.c`, the Hall-of-Fame row displacement is now `3 * max(80 - frontend_fame_intro_tick, 0)` native picture pixels, replacing the R175 piecewise quadratic easing expression. One common function supplies the offset to all five score rows *and* to the name-entry overlay. The earlier 48-VBlank picture fade, 96-VBlank prompt reveal, final row positions, colors, glyphs, rank/score computation, authored ROM resources, title fade, Game Over two eight-tick fades and current input behavior were not modified.

This is a footage-measured motion correction, **not** proof of an instruction-exact Jaguar Object Processor/GPU implementation. In particular the full animation's phase alignment, image brightening curve and exact output of original blitter/OP list remain separately unverified.

`tests/test_frontend_renderer.c` now checks raster coordinates at animation ticks 32, 48, 79 and 80, including a sharp stop in the next interval. The test fails at tick 32 against the original R175 renderer and passes against R176 under GCC and Clang. No instrumentation was inserted into the game.

## Verification

- Strict native Linux GCC/Clang Release builds: **PASS**.
- GCC/Clang focused `test_frontend_renderer`, `test_mjp`, `test_music`, `test_campaign_end`, `test_frontend_fade`: **PASS**.
- GCC/Clang front-end renderer undefined-behavior sanitizer: **PASS**.
- New renderer test against unchanged R175 production renderer: **EXPECTED FAIL** at tick-32 row raster check.
- All original `src/game` source files: **BYTE-IDENTICAL** to R175.
- Full long-running game regression suite, interactive actual-game playthrough and cycle-accurate Jaguar OP trace: **NOT performed**; no 100% parity claim.

## Release policy

Distribute current source, two x86-64 native Linux executables (mode 0755), release notes, historical R001–R176 project addendum, preserved research and existing credits/licenses. No commercial `AVP.jag` ROM, other copyright-protected dumps, private Ghidra project or older release ZIPs. The user's own canonical original 4 MiB `AVP.jag` is the only required ROM (SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`).

# R176 verification — October 22, 2026

| Check | Result |
| --- | --- |
| GCC/Clang strict Release builds | PASS |
| GCC/Clang renderer tests incl. 32/48/79/80 raster checks | PASS |
| New R176 test with unchanged R175 renderer | EXPECTED FAIL at tick 32 |
| GCC/Clang MJP selection/lose CPU test | PASS |
| GCC/Clang music, campaign-end and frontend-fade tests | PASS |
| GCC/Clang renderer UBSan (`-fno-sanitize-recover=all`) | PASS |
| All 33 `src/game` files vs R175 | IDENTICAL |
| Complete, long-running gameplay suite | NOT RUN |
| Full interactive original-vs-PC image/timing comparison | NOT RUN |
| Retail waveform/OP instruction-level exactness | NOT PROVEN |

The recorded vertical rate is measured from BigPEmu video and scaled back to Jaguar-native coordinates; scroll phase length is presentation-calibrated. No compiled runtime diagnostic instrumentation was added.
