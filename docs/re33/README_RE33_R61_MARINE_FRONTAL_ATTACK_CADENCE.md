# Alien vs Predator (Atari Jaguar) — RE #33 R61

R60 baseline: Marine front-facing 3rd-person firing pose refinement only.

- Shotgun: front `AS_FIGHT2` playlist `[0,1,2]`, once per genuine `active_wep` action-start event; removed return pose `[1]` remains excluded.
- Pulse Rifle: front `AS_FIGHT1` playlist `[0,1,2]` cycling while Fire is held; original final stand-up frame `[3]` no longer plays.
- Smart Gun: same frontal playlist as Pulse Rifle.
- Flamethrower: front `AS_FIGHT2` playlist `[0,1,2]` cycling while Fire is held; removes final `[1]` of previous `[0,1,2,1]` cycle.

No damage/weapon fire rate/ammunition/first-person change. The projectile cleanup fix from R60 is preserved. All non-frontal Marine sprite timelines and Alien/Predator sprite timelines are unchanged; damage, death, weapon swaps, and button release still interrupt the appropriate animations.

Native input, original `src/game/` Jaguar code, bot logic, map maker and other settings are unchanged. No ROM included.
