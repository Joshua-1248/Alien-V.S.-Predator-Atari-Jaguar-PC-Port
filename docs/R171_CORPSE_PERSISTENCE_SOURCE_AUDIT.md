# R171 — Source-backed corpse persistence across inter-level travel

## User report / root cause

In R170, a corpse remained visible in its active map but vanished when the player left and revisited the level. The fault is in `src/game/amp.c::ram_save`: a host-added conditional rejected every allocated AMP carrying bit `AMP_DEAD`. This discarded slain enemies from the saved per-level `AvpAmpSavedPlacement` list, regardless of their stationary `host_static` or active dead-Alien acid mode.

## Original references

- The recovered original readable-C source at `Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-C-Decompilation`, `src/game/amp.c::ram_save`, saves records where `mode || host_static` and `astype >= 0`, with **no AMP_DEAD exclusion**. URL: https://raw.githubusercontent.com/Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-C-Decompilation/main/src/game/amp.c
- Death paths in the existing translated `src/game/amp.c` already change an actor's `astype` to dead template #52 (Alien), #53 (human Marine), #54 (Predator), #55 (Facehugger), and queen logic #57. They clear the live collision owner when the entity dies. `ram_save` stores `x>>8`, `y>>8`, `astype`, and `flags`, with an `x=-1` terminator. On revisit, `rebuild_level` instantiates that persisted template and flagword.
- The rebuilt AMP is drawn when `mode || host_static` by `src/jaguar/avp_gpu/software_renderer.c::draw_amp_layer`. `host_static` is the PC representation of original Jaguar stationary mode -1, distinct from empty mode 0.

This is source-C and existing-death-logic evidence. We did not execute a new Ghidra disassembly or claim byte-identical native C behavior in this release.

## Minimal changes

1. Remove the PC-only `AMP_DEAD` exclusion from `ram_save()`. Corpses, active dead-Alien acid objects, living enemies, and uncollected stationary props follow the same original validity check. Consumed items and released AMPs remain excluded as before.
2. Do not put corpse template IDs 52..57 back into `collmap` when `apply_template()` restores a saved corpse. This preserves the original death-time cleared collision registration: dead corpses cannot block the player or absorb shots as live enemies. All non-corpse placement and collision semantics remain untouched.
3. Extend existing `tests/test_amp.c` with saved corpse assertions, every major death template, repeated two-level round trips, no-resurrection, death-frame and flags preservation, active Alien acid, no corpse collision owner, and stationary props/consumed pickups unaffected.

## Precise preservation boundary

- Level transfers use `LeaveLevel -> save_level -> ram_save` and `restore_level -> rebuild_level` without resetting `levels_visit`. The 300-AMP limit and original saved placement format are unchanged.
- New campaign start and retail EEPROM restore deliberately begin fresh; **loading a cartridge save is distinct from walking back to a previously visited level**. The Jaguar's original EEPROM save does not retain every killed random enemy permanently across full savegame reloads.
- `KillBastards` is the original entry-square occupant cleanup. This historical behavior is unchanged: an actor precisely on the entry square may be cleared on arrival under its existing game rules. Do not silently replace original entry logic to fabricate permanence beyond its intended scope.
- Death animation transitions, Acid pool damage, cocooning, Queen scripts, score, object loot, save slots, collision for living actors, ROM asset loading, DSP/TOM presentation, and all PC options remain unchanged.
- Original commercial ROM, extracted sprites/audio, private Ghidra databases, and proprietary wave ROM are not included. Only user's local `AVP.jag` is required.

## Verification

The new regression fails against unchanged R170 under GCC and Clang, and passes on R171 under GCC and Clang. Strict Release builds and focused gameplay, Alien cocoon/real hits, enemy Marine/recoil, level/door, AMP routine and UBSan tests pass. The normal suite run excluding the three previously identified exceptionally slow tests (`test_amp_passage_geometry`, `test_amp_projectile`, `test_hud_sample_columns`) completed successfully under GCC and Clang. The complete uninterrupted monolithic suite including those three was not claimed as fully passed for R171. Real user-side gameplay A/B and BigPEmu parity were not measured here.
