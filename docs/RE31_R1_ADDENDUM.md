# Alien vs. Predator (Atari Jaguar) — RE #31 R1

Baseline: the complete RE #30 R24 source archive. Do not stack this patch onto an earlier R22/R23 or an unverified S116 fork.

## Recovered context

- RE #30 R23 already supplies F2 "Show Other Players On Automap" (on by default, co-op and Deathmatch).
- RE #30 R23 already repairs AI weapon-cycle action bridging for all 16 native actor slots, and grants all four combat Predator weapons in Deathmatch.
- RE #30 R24 already validates bot motion against authored map floor while preserving original `SafePos`/PLAYER.S movement and normal human collision.
- No older bot AI, spawn, map or multiplayer implementation was replaced in RE31 R1.

## New: PC-only fixed HUD hue

F12 -> Miscellaneous -> **Fixed HUD Hue (Ignore Background Color)**.

* OFF (default): original Jaguar CRY RMW, visually dependent on scene color.
* ON: keep the real scene pixel's brightness but use a single neutral (Jaguar QCONV-derived) starting CRY chroma before applying the original active HUD palette/RMW source ink. A HUD pixel over an equally bright blue/red/gray background now has the same resulting hue. This avoids the large HUD hue shift when the view moves from textured flooring to a pitch-black ceiling.
* The source RMW function, HUD palette definitions, brightness controls, static and animated HUD brushes, map, scanner, ammo bars and existing video effects are preserved.
* Changes apply to the local PC HUD renderer only, both solo and each of the four local viewports. No ROM/CPU/player-gameplay changes.
* Setting saved as `misc.hud_fixed_hue=true|false` in `settings.cfg`.

## Verification (no retail ROM available)

* GCC Release full build: PASS.
* Clang Release full build: PASS.
* GCC Release, multiplayer disabled: PASS.
* Native multiplayer executables: 38 PASS, 0 FAIL, 3 SKIP (two retail-ROM-dependent and one X11 display test). The exact skip set and test results are recorded in R1 development logs.
* New ROM-free CRY hue regression: PASS.
* Entire legacy `tests/run_tests.sh`: cannot be counted as a pass; it currently stops at `test_levels_doors` because unchanged `tests/test_levels_doors.c` and `src/game/runtime.c` both define `avp_runtime_ops`. This pre-existing test-harness build issue is unrelated to R1 HUD code.
* Actual gameplay/visual inspection in the Queen's Throne Room: NOT possible without a user-supplied `AVP.jag`. Treat visual results as pending verification.
* Windows/MinGW build: NOT tested here.

## Spawn/corner issue: pending retail replay

A source-level review confirmed that Deathmatch candidate positions are 16.16 cell centers, constrained to authored floor cells, connected components, the `avp_mp_dm_spawn_interior` checks, the original `SafePos(80, x, y, 40)` and original AMP collisions. Co-op follows the original starting pose with fallback checks; R24 additionally guards bot movement into floorless cells. There is not yet evidence that the reported visual corner clipping originates at spawn rather than later movement, interpolation, world geometry or sprite occlusion. Changing original native collisions or making up offsets would risk new failures. Preserve R24 logic until an actual R24/R1 replay identifies level, player and position at the offending frame.

## Apply

From an unmodified R24 `SOURCE` directory:

```sh
patch -p1 < ../AVP_RE30_R24_TO_RE31_R1.patch
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Or use `AVP_RE31_R1_COMPLETE_SOURCE.zip` directly. The Linux build archive includes updated GCC/Clang executables and prototype texture resources, but never the retail Jaguar ROM.
