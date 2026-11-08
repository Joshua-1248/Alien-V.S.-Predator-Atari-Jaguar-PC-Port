# Alien vs Predator (1994 Atari Jaguar) — PC port R177

**Baseline:** R176, October 22, 2026. **Scope:** Game Over / Hall-of-Fame ongoing presentation fidelity.

## Retail comparison follow-up

After the Hall-of-Fame row-motion fix in R176, the remaining obvious frontend discrepancy was the vertical placement of the Game Over `SIMULATION TERMINATED` line. Re-checking the user-provided retail screenshot against the current renderer showed the earlier hosted special-case offset was almost certainly overcorrected: the authored text record already carries its own blank rows, so the line belongs near the top of the image on the normal MJP text grid rather than being pushed deep into the lower half of the screen.

## Narrow implementation

In the PC-only renderer `src/jaguar/avp_gpu/frontend_renderer.c`, the special hosted Y/X offsets for `AVP_MJP_SIM_TERMINATED` small-font text were removed. The renderer now uses the ordinary `14 + line*14` MJP line grid and the original per-glyph X positions supplied by `Show_Tex`, letting the authored blank rows in cartridge text record `$2AFC3` determine placement. The existing two same-page eight-VBlank fades, Hall-of-Fame timing, title/high-score transition behavior, authored ROM strings, and all original `src/game` logic remain unchanged.

`tests/test_frontend_renderer.c` was updated accordingly: it now confirms the first visible `S` appears at native X=63,Y=28, that the incorrect old lower-screen raster stays clear, and that the same two eight-tick fades still black the line out and restore it exactly as before. No diagnostics or instrumentation were added to production code.

## Verification

- Strict native Linux GCC/Clang Release builds: **PASS**.
- GCC/Clang focused `test_frontend_renderer`, `test_mjp`, `test_music`, `test_campaign_end`, `test_frontend_fade`: **PASS**.
- All original `src/game` source files: **BYTE-IDENTICAL** to R176.
- Full long-running game regression suite, interactive actual-game playthrough and cycle-accurate Jaguar OP trace: **NOT performed**; no 100% parity claim.

## Release policy

Distribute current source, two x86-64 native Linux executables (mode 0755), release notes, historical R001–R177 project addendum, preserved research and existing credits/licenses. No commercial `AVP.jag` ROM, other copyright-protected dumps, private Ghidra project or older release ZIPs. The user's own canonical original 4 MiB `AVP.jag` is the only required ROM (SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`).

# R177 verification — October 22, 2026

| Check | Result |
| --- | --- |
| GCC/Clang strict Release builds | PASS |
| GCC/Clang focused `test_frontend_renderer` | PASS |
| GCC/Clang focused `test_mjp` | PASS |
| GCC/Clang focused `test_music` | PASS |
| GCC/Clang focused `test_campaign_end` | PASS |
| GCC/Clang focused `test_frontend_fade` | PASS |
| All 33 `src/game` files vs R176 | IDENTICAL |
| Complete, long-running gameplay suite | NOT RUN |
| Full interactive original-vs-PC image/timing comparison | NOT RUN |
| Retail waveform/OP instruction-level exactness | NOT PROVEN |

The changed production surface is limited to the PC-only frontend renderer. The original game-owned code under `src/game/` was not modified.
