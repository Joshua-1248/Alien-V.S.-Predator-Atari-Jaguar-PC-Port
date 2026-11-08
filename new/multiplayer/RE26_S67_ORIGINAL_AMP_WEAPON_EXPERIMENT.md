# RE26 / Sweep 67 — original AMP + per-player weapon pipeline experiment

## Source-level change and isolation

- Added `--mp-combattest-2` to the existing Linux/PC `avp_pc` experimental command line. The prior `--mp-walktest-2` **does not change** (AMP and weapon-script advancement stay off in that mode). Normal single player is untouched. No Start Co-op menu option is enabled.
- In `local_mp_native_backend.c` an **optional** `advance_actor_post_world` callback runs after one shared-world tick, in a separate, serial `AvpMpActorNativeScope` for each level participant, saving/restoring original player/weapon/HUD fields. This follows the *ordering* of `src/game/maze.c::NextFrame`: `UpdatePlayer()`, `UpdateAMPs()`, then `PreFrame()` -> `UpdtScrOverlays()`. The experiment invokes `UpdtScrOverlays()` directly and does not falsely claim to run every `PreFrame` presentation callback, `SwapScreens`, `PostFrame`, `Process_EndGame`, or `DoPause` step.
- In the combat-only walktest `choose_amp_target` supplies a selected living player for the already existing `avp_mp_amp_step_targets`, so the **original** `src/game/amp.c` mode function executes exactly once per active AMP, not twice per shared level. Target selection uses `max(abs(x_delta),abs(y_delta))` in Jaguar coordinate units, based on the axis-based proximity convention of original `amp_in_range`. Ties choose lower player ID. **This proximity policy is a new, experimental co-op decision, not a verified retail Jaguar multiplayer behavior**; original Jaguar AVP has no such multiplayer policy.
- Weapon scripts and original hit math are not substituted; experimental actor-post calls `src/game/mazescrn.c::UpdtScrOverlays`, which calls `PredAvail` (Predator), `update_wep`, `process_shot`, `recharge` (all original game-side implementations). No synthetic damage is added to retail sessions.
- The two original actor snapshots stay separate. AMP allocator, projectiles, and door simulation remain world-owned. The normal host state is restored at the end of each transaction.

## Actual unmodified retail ROM testing

- Original user ROM 4,194,304 bytes, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`, **not included in deliverables**.
- Real level 1: the original ROM supplies the map, AMP placements, textures and HUD. On the initial combat-mode tick 232 AMP modes were entered; by tick 60 the counter reached 8,905 visits. This includes effects as well as enemies; do **not** interpret it as 8,905 separate attacking creatures.
- Two live source-rendered Marine HUD/views, one X11 window. Linux/Xvfb automated test with independently mapped P2 forward (`i`) and P2 fire (`p`) reports P1 view unchanged in sampled movement phase, P2 viewport changed by 36,184 pixels, P1 `fire=0`, P2 `fire=255` while held, and active original AMP-mode dispatch. P2 input was not redirected into P1.
- **Neither Marine possessed a weapon** in these starting snapshots: each reported weapon slot 0 and ammo0=0. An original firing *input* and original weapon update path were exercised; there was no projectile or confirmed hit or ammo decrease. The player health remained 1000 in this short start-position test. Cannot claim working combat from this test.
- A diagnostic screenshot with some moving AMP objects in P1 is not a regression: when shared enemy AI runs, P1's view may change even if P1's position is fixed. Movement-only visual smoke retains its original strict P1 pixel-identity assertion.

## Verification

- Strict GCC and Clang `avp_pc` build: PASS.
- Strict GCC `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` `avp_pc` build: PASS.
- 18 original multiplayer regression executables pass, including the original-ROM 15-level collision/maze parity and one-window X11 pixel readback. `test_local_mp_native_backend` additionally exercises new optional post-world actor callbacks and host rollback.
- Clang ASan+UBSan `test_local_mp_native_backend` and `test_local_mp_native_context`: PASS.
- `re26_retail_two_view_smoke.py` normal movement-only path: PASS with original ROM. `re26_retail_two_view_smoke.py --combat` independent input/AMP path: PASS with original ROM. These use Xvfb, XTest, and temporary per-test settings and binary copies.
- **Windows build and runtime not verified** in this environment.

## Repeatable commands

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build --target avp_pc test_local_mp_native_backend
./build/avp_pc --rom /your/AVP.jag --diagnostic --mp-walktest-2
./build/avp_pc --rom /your/AVP.jag --diagnostic --mp-combattest-2
python3 new/multiplayer/tests/re26_retail_two_view_smoke.py --exe build/avp_pc --rom /your/AVP.jag
python3 new/multiplayer/tests/re26_retail_two_view_smoke.py --exe build/avp_pc --rom /your/AVP.jag --combat
```

## Not complete / next priority

1. Confirm the original pickup/weapon acquisition path under two actor contexts so an armed Marine can fire. The original first-level spawn provides no weapon or ammunition to either player.
2. Test real original `TestSpark` / `process_shot` contacts and AMP damage/retirement, including player-specific ammo/weapon animation changes and source HUD feedback.
3. Validate and refine new co-op enemy target policy against interactions: ambush, projectiles, player death, visibility, mixed species, object/entity modes. No original Jaguar multiplayer reference exists; retain source combat mechanics and explicitly document new policy.
4. Wire full `PostFrame`/HUD progression, environmental pickup ownership and collisions, correct death/respawn, co-op audio, terminals, level transitions and multiplayer save semantics before Start Co-op is enabled.
5. Four local players and Windows multi-mouse/gamepad runtime not exercised by this experiment.
