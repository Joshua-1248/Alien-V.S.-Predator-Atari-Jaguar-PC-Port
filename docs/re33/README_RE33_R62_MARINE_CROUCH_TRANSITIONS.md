# Alien vs Predator (Atari Jaguar) – RE #33 R62

R61 baseline. Presentation-only Marine front-facing Pulse Rifle and Smart Gun crouch transitions.

Original retail AAF 18 has frames 0..3. Its enemy Marine crouch playback is
`0,1,2,2,3,2,1,0` in `src/game/amp.c::crouch_frames`.
The R61 request explicitly removed frontal frame 3 during held firing. R62
preserves that choice, while staging the existing three AAF frames:

- First held Fire tick: frame 0 (upright/lowering start).
- Second held Fire tick: frame 1 (lowering/crouch).
- While Fire continues: alternate frame 2 (crouched firing) and frame 1;
  neither standing frame 0 nor retired frame 3 occurs in the sustained loop.
- On Fire release: show frame 1, then frame 0 once, and return to idle.
- If the Marine takes damage, dies, or changes weapons: cancel the transition;
  existing pain/death/weapon priority remains authoritative.
- Very short Fire tap before a complete native frame cannot invent a crouch.

Both the native engine and this presentation use 10 Hz simulation ticks.
No ammo, weapon, shot frequency, enemy AI, or original `src/game/` code changes.
Shotgun `[0,1,2]` and Flamethrower `[0,1,2]` behavior remain R61-exact.
Side/rear animations remain unchanged and do not play an added release pose.
Both the P1-P4 local actors and same-species bots use per-actor visual state.

Tests: `new/multiplayer/tests/test_local_mp_sprite_pose.c` now validates both
weapons' entry, sustained, exit, re-press, pain/weapon-change, quick-tap paths,
and keeps pre-existing side/rear and other species assertions.
