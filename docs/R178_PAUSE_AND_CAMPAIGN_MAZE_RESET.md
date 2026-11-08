# Alien vs Predator (1994 Atari Jaguar) — PC port R178

**Baseline:** R177, October 22, 2026. **Scope:** two user-reported lifecycle regressions: stale character-selection artwork behind PAUSED, and doors left open across separate campaigns.

## Root causes and focused corrections

**Pause image:** PC high-resolution 3D rendering writes the live world to `world_fb` and presents it directly. MAZESCRN.S::SaveOvers was passed the native `fb`, which could still contain the previous SELECT briefing. The unchanged Jaguar `PSVO`/`PLNK`/`PRVO` pause renderer therefore saved and displayed the previous frontend pixels. On the existing `PSVO` event only, `src/platform/pc/pc_video_x11.c` now transfers the current high-resolution world framebuffer into the native pause framebuffer with the existing nearest-neighbour scaler before letting the original pause handler save overlays. Normal native-resolution gameplay, framebuffer rendering, pause menus, input and restore logic are unchanged.

**New campaign doors:** `InitPlayer` calls `InitDoors`, which correctly resets its internal level-to-level door-state table. However, the PC resources retain writable `AvpMazeCandidate.decoded` buffers for the duration of the process; open door bytes previously written into that decoded map remained open after Game Over. When `pc_begin_campaign(...,from_title=1)` began a fresh campaign it rebound those dirty decoded buffers, overriding the bookkeeping reset. R178 adds a PC-only, transactional `avp_pc_restore_new_campaign_mazes` routine in `pc_resources.c` that decodes the original maze records from the user's own `AVP.jag`, validates all sizes, and only then restores existing cached allocations in place. The hook runs at new campaign start **before** `InitPlayer/InitDoors` and does not run on level revisits, stage switches or quickload. Existing `SaveDoors`/`ResetDoors` intra-campaign persistence remains untouched. No new ROM or asset is required.

## Verification

- Canonical uploaded original retail `AVP.jag`: **15/15** authored retail mazes recovered and restored; **367/367** deliberately altered real door-state bytes returned to the source-ROM values.
- New synthetic ROM-free tests: multi-map reset, transactional failure without partial overwrite, pause high-resolution world downsampling and Jaguar `SaveOvers`/`RestoreOvers` frame preservation: **PASS** GCC/Clang.
- Existing focused tests: `test_pc_presentation_scale`, `test_jaguar_compat`, `test_levels_doors`, `test_campaign_end`, `test_mjp`, `test_frontend_fade`, `test_frontend_renderer`, `test_quickstate`: **PASS** GCC/Clang.
- Both strict Release Linux native builds: **PASS**.
- Both new focused tests with GCC and Clang UBSan and abort-on-UB: **PASS**.
- All **33 original-derived `src/game` source files** byte-identical to R177.
- Complete long-running test suite and interactive end-to-end pause/campaign restart on the desktop: **NOT** performed in this session; original Jaguar/OP exactness not established by these checks.

## Distribution

Current source, two executable x86-64 Linux builds (GCC/Clang, mode 0755), release notes, cumulative R001–R178 addendum, preserved research, attribution and licenses. Does **not** include commercial game assets or ROM, extra wave ROM, private Ghidra databases, player saves, older release ZIPs or caches. Only original 4 MiB `AVP.jag` is required (canonical SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`).


# R178 verification — 2026-10-22

| Item | Result |
| --- | --- |
| GCC strict Release build | PASS |
| Clang strict Release build | PASS |
| Canonical original ROM 15/15 mazes | PASS |
| 367/367 actual original-ROM door-state bytes restored | PASS |
| GCC/Clang `test_pc_maze_restart` | PASS |
| GCC/Clang `test_pc_pause_world_snapshot` | PASS |
| GCC/Clang `test_pc_presentation_scale` | PASS |
| GCC/Clang `test_jaguar_compat` | PASS |
| GCC/Clang `test_levels_doors` | PASS |
| GCC/Clang `test_campaign_end` | PASS |
| GCC/Clang `test_mjp` | PASS |
| GCC/Clang `test_frontend_fade` | PASS |
| GCC/Clang `test_frontend_renderer` | PASS |
| GCC/Clang `test_quickstate` | PASS |
| GCC/Clang UB sanitizer on new tests | PASS |
| All 33 original `src/game` files against R177 | IDENTICAL |
| Complete long-running regression suites | NOT RUN |
| Live game pause/door retest under actual desktop play | NOT RUN |

The actual ROM was used for verification only and is not packaged. New regression tests are synthetic and ship without proprietary bytes. Changes are restricted to PC host/runtime/resources and tests, with no diagnostics/instrumentation inserted in production.
