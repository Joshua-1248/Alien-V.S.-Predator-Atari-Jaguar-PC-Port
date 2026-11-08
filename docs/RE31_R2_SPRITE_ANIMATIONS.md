# Alien vs. Predator (Atari Jaguar) — RE #31 R2

**Baseline:** RE31 R1 complete source. This revision modifies *only* render-only multiplayer player-avatar sprite selection and the related pose tests/build registration. R1 fixed HUD hue, R23 Predator weapon cycling/deathmatch inventory and automap preference, and R24 bot collision/floor protection remain intact.

## Original-source references

- `src/game/amp.c` (reconstructed AMP.S): `amp_move_toward` increments Alien/Predator `AS_RUN` frame once per native AMP update; enemy Marine AI processes chase on alternate updates. `amp_recoil`/`amp_recover` holds `AS_KNOCKBACK` frame zero for two native ticks. `invis_frames`, `appear_frames`, `fight_swipe`, `fight_bite`, `fight_tail`, `fight_pred_combi`, `fight_pred_knife`, `laser_frames`, `amp_disc_prepare`, `amp_crouch_fire` and `amp_launchfire` are the visual frame authority.
- `src/game/mazescrn.c` (reconstructed MAZESCRN.S): Marine slot 1 = Shotgun, 2 = Pulse Rifle, 3 = Flamethrower, 4 = Smart Gun; Alien slot 1 = Tail, 2 = Swipe (claws), 3 = Bite.
- `src/jaguar/avp_gpu/software_renderer.c` (ROM AAF selector): original enemy Marine pulse (`AS_FIGHT1`) = AAF 18; enemy Marine standing/flamethrower (`AS_FIGHT2`) = AAF 15. Player-only `AS_WALK`/`AS_RUN` facing uses original directional AAF 24 (front), 55 (flanks, mirrored), 56 (rear). Original enemy mapping is unchanged.

## Changes

1. An idle Alien always uses original `AS_STAND`/AAF8 frame zero, rather than advancing its idle pose with global ticks.
2. Motion begins on frame zero when movement starts. Aliens and Predators use one source run frame per 10 Hz native tick; Marines keep their alternate-tick source cadence. Only visual sprite frames change; native movement physics and world AI do not.
3. Alien Tail and Bite avatar animations now map to their actual MAZESCRN weapon slot order (1 Tail/FIGHT3, 2 Claw Swipe/FIGHT2, 3 Bite/FIGHT1). Claw Swipe keeps the exact original AMP.S six-frame source playlist at one frame per native tick.
4. Predator combat animations preserve source AMP.S one-frame-per-tick playlists; smart-disc visual frames now include the source final frame. Predator recoil displays frame zero of the enemy `AS_KNOCKBACK` sheet for two native ticks, triggered by a decrease in that actor's health. The original PLAYER.S pain fade and damage calculations remain unchanged.
5. Predator player cloaking is now a separate render-only timeline using the exact enemy `invis_frames` (including its eleven repeated frame-1 holds, excluding the non-visual sound marker). After 17 visual updates, the actor uses `AS_HIDDEN`; on uncloaking it plays the six source `appear_frames` in reverse-visibility order. The actual player cloak toggles, targeting vulnerability, invisflag, sound and game rules are unchanged. Already-cloaked avatars appearing in a newly observed view start hidden.
6. Marine avatar Shotgun and Flamethrower select the original enemy standing/firing sequence (`AS_FIGHT2`, AAF 15 in the front view). Pulse Rifle and Smart Gun select the original enemy crouched/pulse sequence (`AS_FIGHT1`, AAF 18 in front). Enemy Marine actors are unaffected. For each observer, a stationary Marine with an active shot uses held frame zero from the directional idle sheets on side/rear views; the firing sheet plays when viewed head-on. A moving Marine may still animate directional running frames.
7. Added a separate no-ROM-facing test against the actual renderer sprite-choice routine and expanded the 10-Hz pose/weapon/cloak tests. The facing test uses always-on checks, including in Release builds.

## Scope and verification

- Strict GCC Release full build: PASS.
- Strict Clang Release full build: PASS.
- Strict GCC Release with multiplayer disabled: PASS.
- GCC and Clang multiplayer executable suites: **39 PASS / 0 FAIL / 3 SKIP** each, including the new real renderer-facing test. Skips require a retail ROM or live window/display setup.
- Legacy root `tests/run_tests.sh` remains blocked at an unrelated `test_levels_doors` link error: `avp_runtime_ops` is multiply defined in its test stub and `src/game/runtime.c` (also documented in R1). That is not a sprite regression.
- No original retail `AVP.jag` is distributed or available for final visual replay. **Actual in-game frame pacing, partial cloak transitions, all player orientations, and weapon-shooting appearance still require retail ROM confirmation.** Tests here validate source mappings and executable state, not every on-screen result.
- Windows/MinGW build not verified.

## Package/apply

Apply `AVP_RE31_R1_TO_RE31_R2_SPRITE_ANIMATIONS.patch` to a clean RE31 R1 source **archive root** (the folder containing `SOURCE`):

```sh
patch -p1 < AVP_RE31_R1_TO_RE31_R2_SPRITE_ANIMATIONS.patch
cmake -S SOURCE -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Alternatively use `AVP_RE31_R2_COMPLETE_SOURCE.zip`. Linux archive contains GCC and Clang executables and the retained prototype texture restoration resources. Supply a legally obtained retail `AVP.jag` yourself.
