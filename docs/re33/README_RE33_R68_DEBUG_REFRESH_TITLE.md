# RE #33 R68 — Debug Refresh Level and Return to Title

Base: pristine `AVP_RE33_R67_COMPLETE_SOURCE.zip`.

## F12 → Debug → Refresh Level : PRESS ENTER

- Available only during an active game.
- Restores the current level's writable map from its original retail ROM FILES record, including the original wall/door bytes.
- Replaces that level's previous saved door positions with the refreshed positions before invoking the original door reset logic.
- Clears **only** the current level's saved AMP record and visited bit. Calls the original `restore_level()` first-visit path to rebuild authored actor placements and reroll random actors. Does not call enemy death routines or tally kills.
- Preserves player position, health, weapons, inventory, campaign progress, and all other levels' saved actors and doors.
- In local multiplayer the command targets Player 1's current active world, and is executed once inside that world's native simulation context. Local split-screen worlds that the selected room does not match are untouched.
- Deathmatch with Enemies Off continues respecting its enemy-suppression setting.
- Requires the authentic retail ROM map source. If unavailable, refresh is rejected without altering the level.

**Scope:** Refreshes a level, not the campaign. Existing campaign progression flags and saved player state do not reset. This does not create a new game or replace the dedicated reset/restart controls.

## F12 → Miscellaneous → Return To Title Screen

- Ends the active game or local multiplayer session, cleans up audio/session state, and calls the original `NewTitle` frontend.
- Single-player **and** local multiplayer now suppress any held A/B/C confirmation from the initiating F12 command until release. A new, separate press is necessary to select New Game.
- Clears queued F12 actions and buffered confirmation input on the title transition. No automatic restart of the previous game.

## Verification

- Full multiplayer-enabled optimized GCC and Clang Linux builds: PASS.
- New `test_amp_refresh_level_r68` validates re-creation of current level's authored actor list from first-visit state while keeping another level's saved actors and visit flag.
- New `test_f12_debug_refresh_r68` checks root/menu routing, repeatable press action, pending-action isolation, Return To Title action, and title input latch reset.
- Both new tests: GCC/Clang PASS; Clang ASan + UBSan PASS.
- Existing R60 flame/projectile collision, R65 enemy removal, R67 hazard removal, R51 bot/awareness/navigation, Deathmatch, Queen campaign, actor state, Map Maker 841, AMP playtest 79, native door-flow 42, F2 title attract 7: PASS GCC and Clang.

Actual interactive retail-ROM refresh and title input behavior should still be confirmed on a player's system; automated tests are not a substitute for in-game observation. Windows/MinGW not compiled here. The retail ROM is **not** bundled.

## Linux archive layout

`LINUX/avp_pc` — GCC, multiplayer-enabled
`LINUX/avp_pc_clang` — Clang, multiplayer-enabled

The files have Unix executable mode `0755` in ZIP and TAR.GZ distributions.
