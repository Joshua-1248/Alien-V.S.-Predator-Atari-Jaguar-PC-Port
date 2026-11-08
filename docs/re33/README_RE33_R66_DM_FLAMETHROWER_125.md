# Alien vs Predator (Atari Jaguar) — RE #33 R66

Baseline: the complete RE33 R65 source.

Deathmatch PvP Marine flamethrower impact damage per flame projectile is now **125 HP** (previously **75 HP**). Only the Marine's projectile-vs-player damage table entry (original `AC_FIRE`, enum 8) changed. The native flame-to-enemy and explosive-barrel damage, projectile generation, collision-grid ownership fix from R60, damage to players from hostile flames, ammunition, and weapon cadence remain exactly as in R65.

The Deathmatch regression assertion was updated to 125. GCC and Clang strict Release builds and focused Deathmatch plus R60 projectile, bot, awareness, navigation, actor and sprite-pose tests passed. No original `src/game/` file changed. Windows and interactive retail-ROM gameplay were not tested.
