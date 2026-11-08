# RE #33 R60 — Projectiles preserve impacted actor collision registration

## Problem

In R59, `src/game/amp.c` `release_amp()` called `amp_cleargrid_c()` unconditionally on the coordinates of a pooled AMP. When a Marine flame hit a living enemy or an explosive DRUM, the projectile reached its impact coordinate (the same square as the actor), and the projectile's release erased that actor's collision grid entry. Later flames or discs could therefore miss an actor that was still alive and visible.

## Minimal correction

In the host C translation, `release_amp()` only clears its current square if that square is registered to the AMP being released. The existing `amp_cleargrid_c()` semantics for deliberate actor-grid removal are unchanged. `weapon_hit()` values and original Marine flamethrower projectile timing, burst count, collision widths, attack damage, ammo, actor health, barrel explosion, enemy behavior and movement are unchanged. Applies to solo and native local multiplayer because both use the same original AMP core.

## GAS correction

`AC_GAS` denotes flamethrower ammunition; it is not interchangeable with `AC_DRUM`. The R60 gameplay fix does not change the gas pickup. The new barrel regression verifies definition 20 (`DRUM`) and tests living Alien, Marine and Predator enemies separately. No claim is made that the ammo pickup is an explosive barrel.

## Regression test

`test_amp_flame_barrel_release_r60` (included in CMake for Unix GCC/Clang) creates live original AMP actors in the source collision grid, performs two actual Marine flame impacts, releases each flame through the original projectile mode, and verifies actor HP and collision-grid ownership after every impact. It also tests two Smart Disc impacts on a living enemy. The test fails against unmodified R59 on the first flame-release grid check and passes on R60. Uses test-only runtime stubs, without modifying game behavior.

## Unverified

Interactive retail-ROM gameplay and Windows/MinGW compilation. Automated GCC and Clang builds and regressions passed; see external release notes.
