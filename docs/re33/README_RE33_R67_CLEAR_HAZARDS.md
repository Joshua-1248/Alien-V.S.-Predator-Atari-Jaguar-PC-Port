# RE #33 R67 — Clear Hazards From Level

Base: the unmodified RE33 R66 complete source archive.

## Cheat

**F12 → Cheats → New → Clear Hazards From Level : Press Enter**

This is a **one-shot action**, not an On/Off setting. It uses the same
**Cheats → Activate For** Player 1–4 selector as **Clear Enemies From Level**.
The action executes only against the selected player's currently bound level.
In local multiplayer, players sharing that level see the same cleared world;
other levels are not altered.

## What disappears

- Explosive barrels: native `AC_DRUM` with retail template **20**.
- Predator-ship electric pyramid/Queen-shield actors: `AC_AQSHIELD`.
- The related shield generators (`AC_GEN`), to prevent recreating the removed
  shields and to stop the generator force-field alarm.
- Already-active damaging Alien acid puddles. Detection uses the original
  `amp_acid` mode (`avp_amp_is_acid_puddle`), not a generic `AC_ALIEN` type test.

Native death routines are **not** invoked. No explosion, enemy hit/death,
kill score, acid spawn, damage to players, generator death/damage event,
Queen death, or campaign progression is triggered. A completed clear is
idempotent (pressing Enter again removes zero actors).

## What remains

- The Alien Queen and all ordinary enemies, eggs and Facehuggers.
- All human players and bots, living Alien actors and their death animations.
- Collectables, including `AC_GAS` flamethrower ammunition, health, and
  weapons, and unrelated furniture and map geometry.
- Retail template **22**, labeled GAS but aliased to drum_def in the current
  source translation. **Do not classify GAS as a barrel.**
- Hostile projectiles that are not permanent level hazards.

Owner-marked collision and object-grid entries are erased **only when they
belong to a removed hazard**. Other collision owners and `$ffff` sentinels
are preserved. Removed AMP slots are returned to the normal allocator; a
subsequent level save omits them. The cheat does not change normal gameplay
or any Jaguar behavior until explicitly invoked.

## Files

- `src/game/amp.c`, `include/amp.h`: PC-only hazard release helper.
- `include/pc/pc_video.h`, `src/platform/pc/pc_video_x11.c`: action bit,
  Enter-only menu item, selected-player routing.
- `src/platform/pc/pc_main.c`: single-player action in current world.
- `new/multiplayer/local_mp_walktest.c`: selected-player current-level world
  action, alongside the previous Clear Enemies action.
- `tests/test_amp_clear_hazards_r67.c`, `CMakeLists.txt`: direct hazard,
  collmap/objmap, Queen safety, pickups, acid and allocator regressions.
- `tests/test_pc_show_fps_menu.c`: real F12 menu action for P1 and P4.

## Verification

GCC and Clang strict Release full multiplayer builds and targeted regression
suite pass, including existing R51 bot, R60 flame/drum, R65 enemy clear,
Queen campaign, Deathmatch, Map Maker (841), AMP overlay (79), native doors
(42), and F12 menu routing. Focused hazard test passes Clang ASan+UBSan.
Linux executables are ELF with 0755 permissions; no ROM ships in archives.
Windows cross-compile and interactive in-game verification remain outstanding.
