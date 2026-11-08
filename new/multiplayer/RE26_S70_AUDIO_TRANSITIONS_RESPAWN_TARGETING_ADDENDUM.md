# RE26 Sweep 70 — source-grounded multiplayer audio, exit ownership, respawn rules, target lock

Source baseline: Sweep 69; no original Jaguar ROM is included. The original retail ROM and original Jaguar assembly/TRM Rev8 remain authoritative for the single-player engine. New multiplayer rules below are *user-requested policies*, not falsely attributed to the original cartridge.

## Implemented and wired to the experimental 2–4-player retail-ROM combat mode

1. **Original CPU weapon voice state per player.** `src/game/music.c` captures/swaps only CPU voice 1 (`AvpMpWeaponAudioState`). The original eight-voice shared allocator, effect metadata, identification counter, alarm voice, music, ambient effect pool, and original sound event selection remain unchanged. The per-actor scope brackets original `UpdatePlayer` and `UpdtScrOverlays` transactions in `new/multiplayer/local_mp_walktest.c`.
2. **Independent host playback slots.** When multiplayer is compiled in, `src/jaguar/host/fullsynth_host.c` extends original 16-slot hosted PCM pool to 19, directs P1 reserved CPU weapon voice to slot 2 and P2–P4 to 16–18, and preserves shared effects and music. Other original CPU voices and their dispatch stay as before. Without multiplayer compiled, the 16-slot layout is unchanged. `avp_jaguar_fullsynth_mp_weapon_start_count` is an instrumentation counter, not an audio synthesis substitute.
3. **Retail original ROM evidence.** The actual original-asset PC executable was run with four marine shotguns and simultaneous input. The repeatable `re26_retail_multi_view_smoke.py --combat --audio-check` checked original weapon effect dispatch to *all four players* (counts `[3, 3, 3, 7]` for tested interval), per-player ammo updates, real per-view movement and single-window presentation. Counts establish correct event ownership/routing, **not** validated speaker/headphone audio, final level mixing or Jaguar hardware audio parity.
4. **Preserved behavior with feature off.** `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` strict build succeeds; no `avp_*_mp*` symbols leak into that executable. No change to original single-player sound logic.
5. **Target choice (experimental only).** An AMP slot's generation identifies an existing enemy. While a target is living and has unobstructed *original* `LineOfSight`, target is sticky. On invalid/dead/obscured target, choose nearest visible living player by original AMP axis-distance convention; when no player visible, choose nearest living player. Iterate one original AMP update per world tick under the selected player scope; no damage multipliers or original AMP behavior changes. This is the agreed provisional multiplayer-specific selection rule, not a reconstruction of single-player targeting.

## Level transitions: implemented request ownership; NOT implemented destination load

- In `walk_actor`, original global `new_level/new_x/new_y/use_cocoon` is read in the owning actor's callback and captured as a `AvpMpTransitionRequest` in a per-player four-slot queue. Conflicting pending requests cannot silently overwrite earlier requests. This prevents P2–P4 exits being attributed to P1. Original `new_x=-1` start fallback remains unchanged.
- `new/multiplayer/local_mp_transition.[ch]` provides queue/peek/commit with bounds and tests. It **does not** load a destination map or consume requests on its own; `walk_create_level` still leases only its existing initial retail map. A requested exit currently causes the walktest to stop with a specific message rather than silently changing another player's level. Unowned level-wide lift exit also fails rather than guessing which player rode the lift.
- To enable real simultaneous transitions: source-verified `LoadLevel` factory must load *each* destination's original map, AMP placements, door state, local HUD-world state, collision geometry, and level specific GPU/textures, and maintain independent resident worlds. Each transfer must use native session `avp_mp_session_move` transaction without rewriting other players' worlds. Timed lift exits require player/lift occupancy owner, not just global new_level. Also handle same-level exits and saved game/campaign semantics.

## Respawn: policy coded/tested but NOT applied live

- Marine: restart original Marine starting level 3, reset inventory; if F12 Marine Start Shotgun toggle is on, enable original shotgun and 44 rounds on the subsequent fresh-start initialization, otherwise no free shotgun.
- Predator: restart original Predator starting level 14, reset inventory, set honor score to zero.
- Alien: if the original oldest cocoon slot is ready (`frame==17`), select that cocoon's native level and original map-centred coords, preserving the cocoon route. The actual original `UseCocoon` still must execute to consume it correctly. If none ready, restart original Alien starting level 15.
- `avp_mp_respawn_plan` only returns this decision. The native loader/`InitPlayer`/`UseCocoon` pipeline must still execute the actual respawn; current walktest still stops on `game_over` and `launch_flag`.

## Outstanding before calling the user request fully done

- Real cross-level original resource factory, multiple simultaneously resident level contexts, lift ownership, the actual original campaign transition path, and the real per-actor respawn application.
- 2–4 player audio in *different* levels: ownership of per-level ambience/music and world listener state, spatialization/mixing policies, pause/resume priority, and physical multi-device runtime confirmation. One shared ambient/music source is correct only for the *single-world experiment*, not the final independent-world campaign.
- Real multi-player enemy hits, enemy/player and player/player collision, death/respawn/Alien cocoon cases, mixed-species campaign, and Windows runtime and hardware verification.
- Full Jaguar hardware audio parity remains outside this co-op-specific change; see existing project fidelity ledgers and TRM Rev8 comparisons.

**Playable status:** Existing retail 2–4 player movement + original AMP/shotgun experimental smoke passes on Linux; no full campaign co-op and Start Co-op remains gated. User ROM/SDK/BigPEmu materials are not redistributed.
