# Alien vs Predator (Atari Jaguar) — RE #33 R70

Base: exact R69 complete source, including R69 F2 Backspace and original-width diagonal bot melee.

## Problem investigation

- `src/game/player.c::MovePlayer` uses one endpoint `SafePos` query (with X/Y slide fallback). Fast Movement in Deathmatch doubles the native maximum speed and acceleration, without creating extra game ticks.
- Original `src/game/collision.c::SafePos` does not require the target maze cell to contain an authored floor selector. The host callback's existing floor guard was applied only to bots, not human P1–P4 participants.
- Deathmatch placement checks the center of an authored floor cell; initial campaign positions and transfer/respawn positions were not validated against the *entire* player-sized floor footprint.
- Fast Movement does NOT directly change the RNG spawn picker. It can expose missing between-endpoint collision validation during movement, and the missing human floor guard can allow players to leave playable space with or without Fast Movement.

## PC-only correction

1. Add host-only `local_mp_move_guard.c/h`, with signed 16.16 full-footprint floor validation using the original 8-byte retail maze cell's byte 5 and the original player half-width `40<<9`.
2. Validate the full footprint at every multiplayer initial position, Deathmatch random candidate, level-transfer arrival, and respawn candidate. Existing original `SafePos`/AMP collision and connected-area selection remain in charge; no positions are teleported to invented locations.
3. Apply a valid-floor footprint check to human P1–P4 in the `pc_runtime.c` callback only while their original multiplayer `MovePlayer` tick is running. Retain the existing center-only bot floor check and all noclip semantics.
4. In Deathmatch when Fast Movement is On, sample the actual original `SafePos` and floor rules along each candidate movement segment, at intervals of no more than 1/8 map cell. The original `MovePlayer` still makes its sole final movement decision and slide fallback. There is no speed cap, physics-rate adjustment, new movement command, wall change, or gameplay-rate change.
5. The scoped guards are cleared after each actor tick (including post-actor cleanup). Single-player and Deathmatch Fast Movement Off have no swept-path checks; ordinary multiplayer human movement retains the new floor-only guard.

## Regression checks

- New `test_local_mp_move_guard`: player footprint at floorless edges, out-of-bounds, crossed floorless regions, crossed original-style wall boundary, unobstructed diagonal, impossible displacement fail-closed, and no-motion case.
- `test_local_mp_retail_rom`: original authored start coordinates all fit within retail floor footprints across all 15 ROM levels, using the real retail RDB.DTA start records.
- Both GCC and Clang strict optimized multiplayer builds PASS.
- GCC and Clang: movement guard, retail ROM, original collision, native player-step, Deathmatch, bot, bot levels/awareness, actor/AMP/native context, respawn, Queen, Map Maker (841), AMP playtest (79), and door-flow (42) tests PASS.
- Clang AddressSanitizer and UndefinedBehaviorSanitizer focused movement guard PASS.
- Original `src/game/` code from R69 remains byte-for-byte unchanged.

## Remaining hands-on verification

This release hardens a **confirmed code-path gap**, but does not claim reproduction of every location where a person may have appeared stuck inside a wall. Test live Deathmatch Alien with Fast Movement On, walk/strafe at walls near level edges, then repeat Fast Movement Off; test random respawns and level transitions with 1–4 players plus bots. Compare any remaining bad position using the GPS cheat coordinates and level number. Windows cross-compilation is outstanding.
