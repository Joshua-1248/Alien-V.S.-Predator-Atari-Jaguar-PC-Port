# Alien vs Predator (1994 Atari Jaguar) — PC port R179

**Baseline:** R178, October 22, 2026. **Scope:** original vent/duct and computer-entry fades on the PC host.

## Original Jaguar behavior and previously invisible fades

The recovered original `MAZE.S` logic in `src/game/maze.c::PlayAvP` calls `FadeDown()` whenever a passage goes into or out of a duct and separately when a computer panel opens. `FadeDown()` programs the existing `MAZESCRN.S::MazeList` fade level to run from zero to `$80`, by `+2` each display VBlank, before a new level or computer screen takes over. The original level movement, door state, terminal interaction and fade-back are already implemented.

The PC host performed level/terminal transitions synchronously and did not continue to present the outgoing gameplay image throughout these VBlank fades. Thus the CPU-owned fade state advanced but the player could see an immediate black/terminal/next-level transition instead of the original gradually darkening picture.

## Narrow PC-only correction

- **Vents/ducts:** on original `FadeDown`, copy the current native or high-resolution gameplay image once. During the 64 original VBlank steps, present that stable outgoing image using the existing Jaguar CRY/QCONV game-CLUT modifier and then the final black endpoint *before* releasing the old level. This avoids recomputing an expensive 3D frame for every transition step and does not consume extra gameplay simulation ticks. The incoming level uses the original fade-in state already present in the PC port.
- **Terminals/computers:** the existing host `enter_retail_computer` calls `FadeDown()` and waited until `faded`, but never displayed the outgoing world while advancing. It now uses the same captured-gameplay fade and explicit final-black endpoint before calling the unchanged `Computer()`. Terminal control, text, audio events and its existing fade-in on exit are untouched.
- The host video capture/presentation methods were named generically as `avp_pc_video_begin_gameplay_fade` / `avp_pc_video_end_gameplay_fade` because they now serve both original transition types. No original 68000-derived gameplay source, maze data, save format, doors, or global game timing was changed. No invented fade curves, rates or additional ROMs were introduced.

## Verification boundary

- GCC strict native Linux Release build: **PASS**.
- Clang strict native Linux Release build: **PASS**.
- GCC/Clang focused `test_pc_maze_restart`, `test_pc_pause_world_snapshot`, `test_pc_presentation_scale`, `test_jaguar_compat`, `test_levels_doors`, `test_campaign_end`, `test_mjp`, `test_frontend_fade`, `test_frontend_renderer`, `test_quickstate`: **PASS**.
- GCC and Clang undefined-behavior sanitizer tests for Jaguar CRY/framebuffer and MAZE campaign-ending fade state: **PASS**.
- All **33 original gameplay `src/game` files** byte-identical to R178.
- A previous canonical-`AVP.jag` unattended X11 boot run confirmed retail ROM reading and resource recovery, but **did not reach a vent or a terminal**, so does not verify the visible transitions interactively. Live vent/computer timing and exact hardware visual equivalence remain **unverified**; the 64-step CPU rate is original-derived, while the frame snapshot is a host presentation implementation.
- Full long-running gameplay regression suite: **NOT RUN**.

## Distribution

Current source, GCC and Clang native x86-64 Linux executables (both mode `0755`), release notes, the complete R001–R179 historical addendum, preserved research and attribution/licensing. No commercial `AVP.jag`, additional wave ROM, proprietary extracted game assets, saves/caches, private Ghidra databases, or older release archives are included. The user's existing canonical original 4 MiB `AVP.jag` (SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`) remains the only required ROM.


# R179 verification — October 22, 2026

| Check | Result |
| --- | --- |
| GCC strict Release build | PASS |
| Clang strict Release build | PASS |
| GCC/Clang focused `test_pc_maze_restart` | PASS |
| GCC/Clang focused `test_pc_pause_world_snapshot` | PASS |
| GCC/Clang focused `test_pc_presentation_scale` | PASS |
| GCC/Clang focused `test_jaguar_compat` | PASS |
| GCC/Clang focused `test_levels_doors` | PASS |
| GCC/Clang focused `test_campaign_end` | PASS |
| GCC/Clang focused `test_mjp` | PASS |
| GCC/Clang focused `test_frontend_fade` | PASS |
| GCC/Clang focused `test_frontend_renderer` | PASS |
| GCC/Clang focused `test_quickstate` | PASS |
| GCC/Clang UBSan Jaguar CRY/framebuffer and MAZE end fade | PASS |
| All 33 original `src/game` files vs R178 | IDENTICAL |
| Prior original retail-ROM unattended startup | PASS through resource load, NOT a fade test |
| Interactive vent/terminal visual comparison | NOT RUN |
| Complete long-running regression suites | NOT RUN |
| Exact Jaguar OP/CLUT pixel parity | NOT PROVEN |

The previously absent visible fade is a host-only presentation change. No production diagnostic instrumentation was added.
