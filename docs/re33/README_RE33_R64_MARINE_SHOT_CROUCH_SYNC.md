# Alien vs Predator (Atari Jaguar) — RE #33 R64

Base: **R63 complete source**. This is a strictly presentation-only correction for the **front-facing, third-person Marine** in local multiplayer.

## Shotgun timing

- Front-facing shotgun visual playlist is back to exactly **[0, 1, 2]**, one run per genuine shot. The extra final frame introduced in R63 is removed. Side/rear playlists remain as in R63.
- The original Jaguar weapon script executes in `walk_post_actor()`, after the player input/movement callback. Before R64, the third-person shotgun pose observed `active_wep` inside `walk_actor()`, **before** that original weapon script ran. This could show a shot before it fired.
- R64 samples the **original MAZESCRN weapon state immediately before and immediately after `UpdtScrOverlays()`**. Only a completed shotgun ammunition transaction starts the three-frame pose. Merely holding Fire, entering an action, switching weapons, or being empty cannot start a fake shot. The original Banana Stops final-round refill (increasing ammo) is also accepted.
- No extra weapon updates or shots are generated. Shotgun hit logic, damage, ammo usage, sound, firing cycle, and first-person animations are unchanged.

## Pulse Rifle / Smart Gun startup

- A newly ready, held-Fire rifle/smartgun now visibly settles into its crouch before the accelerated sustained firing loop begins: **[0, 1, 1]** at the original 10-Hz native visual cadence, followed by the existing crouched **[2, 1]** cycle.
- Pulse Rifle's sustained loop stays **2×** (20 visual frame changes/s) and Smart Gun's stays **4×** (40 visual frame changes/s). Only presentation interpolation advances those pose changes; the native game remains 10 Hz.
- On Fire release, the pre-existing **[1, 0]** stand-up transition plays once. Pain, death, or weapon switching interrupts the visual pose. A held button during weapon drawing cannot start the crouch early.
- The excluded unwanted fourth/standing frame does not reappear during sustained firing.

## Scope and verification

- Existing files changed: `new/multiplayer/local_mp_sprite_pose.h`, `new/multiplayer/local_mp_walktest.c`, `new/multiplayer/tests/test_local_mp_sprite_pose.c`, `new/multiplayer/tests/test_local_mp_weapon_native.c`. One R64 README added.
- All `src/game/` files (including native `mazescrn.c`), R51 bot algorithms, projectile correction from R60, and other Marine weapons are byte-for-byte unchanged.
- Strict Release GCC and Clang executable builds PASS.
- Under both compilers: sprite-pose test PASS; original MAZESCRN shotgun-script transaction test PASS; facing, native actors/player steps, bot, bot levels, bot awareness, Deathmatch, audio mix/layout, Map Maker, AMP playtest, door flow, R60 flamethrower/barrel ownership PASS. Clang sprite-pose AddressSanitizer and UndefinedBehaviorSanitizer PASS.
- Visual presentation in an interactive retail-ROM game and Windows/MinGW compilation **remain unverified**.

## Release layout

- `LINUX/avp_pc`: GCC x86-64 Linux binary, multiplayer enabled.
- `LINUX/avp_pc_clang`: Clang x86-64 Linux binary, multiplayer enabled.
- Both executables stored as regular files with Unix **0755** permissions in ZIP and tar.gz releases.
- This package does not redistribute any Atari Jaguar ROM. Use your legally owned retail ROM with the established game configuration.
