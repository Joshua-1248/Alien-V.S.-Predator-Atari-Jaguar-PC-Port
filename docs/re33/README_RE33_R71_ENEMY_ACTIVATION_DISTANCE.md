# Alien vs Predator (Atari Jaguar) — RE #33 R71

## Extend Enemy Activation Distance

F12 → Enhancements → **Extend Enemy Activation Distance** (Left/Right):

| Mode | Native chase-pursuit range per X/Y axis |
| --- | --- |
| Original (Jaguar), default | 1,152 units |
| 2x | 2,304 units |
| 3x | 3,456 units |
| 4x | 4,608 units |
| Unlimited | No pursuit-distance cutoff in the currently simulated level |

The exact original 1,152-unit world-axis-aligned square test is retained when mode 0 is selected. The other numeric modes multiply **only** this native `amp_chase_target()` pursuit wake check; no AI tactics, animation pacing, LOS, melee or ranged attack ranges, pathfinding, physics, player controls, or drawing distances are modified. The Alien Queen already takes its own original path that bypasses this proximity gate; its behavior is preserved. The Unlimited mode cannot make AI active on a level that is not currently loaded/simulated.

The setting is persistent in `settings.cfg` as `enhancement.enemy_activation_distance=0..4`. Invalid values fall back to Original. It is applied on the native simulation tick in both single-player and local multiplayer, with no change to the 10-Hz simulation rate. Default mode remains the original Atari Jaguar behavior.

Tests: `test_amp_enemy_activation_r71` validates all four edge boundaries and Unlimited; `test_f12_enemy_activation_r71` validates menu cycling, config persistence, and invalid config fallback. GCC/Clang strict Release and regression tests include the R51 bot AI, Deathmatch, R60 projectile collision, R65/R67 clear-level cheats, R68 refresh, Marine sprites, Map Maker, AMP playtest, native door-flow. Original retail ROM reached title under Xvfb, not a live distant-enemy combat test.

Use `patch -p1` inside an **unmodified R70 SOURCE** directory for the R70 → R71 patch, or use the complete R71 SOURCE directory. Linux binaries are `LINUX/avp_pc` (GCC) and `LINUX/avp_pc_clang` (Clang), both including multiplayer and packaged executable with 0755 permissions. No ROM or proprietary assets are redistributed.
