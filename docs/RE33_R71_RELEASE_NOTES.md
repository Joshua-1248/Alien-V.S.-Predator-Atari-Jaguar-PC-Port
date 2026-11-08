# Alien vs Predator (Atari Jaguar) — RE #33 R71

Base: **R70**. No other R70 gameplay features were intentionally altered.

## New F12 Enhancement: Extend Enemy Activation Distance

Left/Right cycles: **Original (Jaguar) (default), 2x, 3x, 4x, Unlimited**.

This changes only the original `amp_chase_target()` chase/pursuit deactivation gate. The native Jaguar gate is 1,152 world units along each X or Y axis, a world-aligned square. Options 2x/3x/4x raise it to 2,304 / 3,456 / 4,608 units, respectively. Unlimited bypasses that gate for enemies in the currently simulated level; it does not simulate other unloaded levels.

The original line-of-sight, attack proximity, enemy speed, weapon damage and gameplay simulation cadence remain intact. Original (index 0) retains the original Jaguar cutoff; this setting persists in `settings.cfg` as `enhancement.enemy_activation_distance=0..4`. Invalid values safely fall back to Original.

The existing game code uses one host-only setter for both single-player and multiplayer tick paths. No modifications to R51 bots, R60 projectile logic, R70 spawn or movement guard, renderer draw distance or graphics were made. One original-derived C game module (`src/game/amp.c`) has a narrowly gated new PC enhancement; other original Jaguar code is unchanged.

## Verification (performed)

- Strict Release `avp_pc` GCC and Clang: PASS (multiplayer enabled).
- R71 native activation distance boundaries: PASS GCC/Clang at Original, 2x, 3x, 4x and Unlimited, with invalid-mode fallback.
- R71 F12 settings and persistence: PASS GCC/Clang (Left/Right, reload, invalid value fallback).
- R71 activation test: PASS GCC/Clang with AddressSanitizer and UndefinedBehaviorSanitizer.
- Bot, bot levels, awareness, Deathmatch, Marine sprite tests: PASS both compilers.
- Original projectile impact, enemy clear, hazard clear, level refresh, Debug return tests: PASS both compilers.
- Map Maker: **841 PASS** per compiler.
- AMP Map Maker playtest: **79 PASS** per compiler.
- Native door flow: **42 PASS** per compiler.
- Retail ROM SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b` reached Jaguar title in 8-second Xvfb smoke; termination by test timeout, audio hardware unavailable.

## Not yet verified

Real-time interactive gameplay with distant AI across varied maps, performance impact of Unlimited with high enemy counts, and Windows cross-build. This is not a confirmation that every enemy mode or distant tactical decision has changed—only the native chase-pursuit distance gate is modified.

## Deliverables

- `AVP_RE33_R71_COMPLETE_SOURCE.zip` — full `SOURCE/` tree.
- `AVP_RE33_R71_LINUX_BUILDS.zip` — `LINUX/avp_pc`, `LINUX/avp_pc_clang`, `LINUX/README.txt` (executable mode 0755).
- `AVP_RE33_R71_LINUX_BUILDS.tar.gz` — alternate Linux delivery preserving native permissions.
- `AVP_RE33_R70_TO_R71_ENEMY_ACTIVATION_DISTANCE.patch` — incremental patch; apply with `patch -p1` inside an unchanged R70 `SOURCE/` directory.
- `AVP_RE33_R71_BUNDLE.zip` — all the above plus this note and SHA-256 manifest.

User-supplied ROMs are not distributed.
