# Alien vs Predator (Atari Jaguar) — RE #33 R63

Base: **R62 complete source**. Local multiplayer third-person Marine presentation only.

## Front-facing Marine animations

* Shotgun: one native `MAZESCRN.S::start_action()`-bound 4-step pose `[0,1,2,1]` per actual shot. It never loops just because Fire is held.
* Pulse Rifle: retain the existing **one-time** crouch `[0,1]` and release `[1,0]`. While firing, alternate `[2,1]` at **twice** the R62 visual speed (20 presentation frame changes/s, versus R62's 10).
* Smart Gun: identical entry and release transitions. Alternate `[2,1]` at **four times** the R62 visual speed (40 presentation frame changes/s).
* Flamethrower: R61/R62 front loop `[0,1,2]` unchanged.
* Before drawing any Marine attack pose, inspect the *original native weapon script* readiness: the current weapon must exist, with no pending new weapon, deselection (`wep_desel`) or move-in/move-out (`mp_equip_phase`). Fire held during equip does not animate the Marine early. As soon as the weapon finishes drawing, normal input resumes; no gameplay Fire event is dropped.
* Damage/death and switching weapons still interrupt cosmetic firing. Original side/rear animation lists, non-Marine poses, first-person weapon scripts, projectile damage, AI, and gameplay timing are unchanged.

## Why animation speeds use sub-tick sampling

The original Atari Jaguar simulation remains **10 Hz**, and the PC presents rendered frames at a faster display rate. R63 samples `presentation_alpha` from the 10-Hz interval for the 20/40-Hz *visual* cycle only. It does not add simulation ticks, perform additional shots or affect ammo or sound. At low display refresh rates, visual frames are sampled as often as the display allows.

## Source changes

* `new/multiplayer/local_mp_sprite_pose.h` — fourth shotgun pose; native weapon-ready gate; sub-tick pulse/smart crouch firing sampler.
* `new/multiplayer/local_mp_walktest.c` — gate attack poses using captured original `AvpMpWeaponEngineState` and sample visual firing phase per display frame.
* `new/multiplayer/tests/test_local_mp_sprite_pose.c` — shot boundaries, ready/equip states, 20/40-Hz render-phase samples, pain/death cancel, non-Marine parity.

All game changes are PC-side; no files under `src/game/` or existing bot behavior are modified. The R60 projectile collision ownership fix remains intact.

## Verification

Full `avp_pc` GCC/Clang Release builds PASS. `test_local_mp_sprite_pose` GCC/Clang and Clang ASan+UBSan PASS. Both compilers: bot, bot levels, bot awareness, Deathmatch, native actor, player step, audio, Map Maker (841), AMP playtest (79), door-flow (42), R60 flamethrower/barrel/projectile regression and Marine sprite-facing tests PASS.

R62 → R63 patch applies cleanly. Linux binary files retain regular-file mode **0755** in ZIP and tar.gz. Interactive in-game visual confirmation and Windows cross-compilation were **not** performed here.

## Linux release layout

`LINUX/avp_pc` — GCC, multiplayer enabled.
`LINUX/avp_pc_clang` — Clang, multiplayer enabled.

No ROM distributed; supply your legally owned original Atari Jaguar ROM as previously configured.
