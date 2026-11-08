# RE #26 / Sweep 63 — Original Jaguar HUD context isolation

Baseline: RE26 S62 complete source. This is an optional co-op native-state
extension, NOT a playable four-player release. Compiles in the existing PC
executable and remains excluded by `AVP_BUILD_LOCAL_MULTIPLAYER=OFF`.

## Actual original-source changes

- `src/game/hud_message.c`: feature-gated, end-of-file snapshot implementation
  of original `HUD_MSG.S` message pointer/timing machine. Includes the mutable
  7-record pickup message, pickup-card string, security access-code text,
  new-level/duct notice strings, and countdown display text. A message's live
  pointers still point to the original source-owned static command arrays;
  the backing strings and records are exchanged during each actor visit.
- `src/game/hud.c`: feature-gated, end-of-file snapshot implementation of
  player-specific tracker, energy meter, screen map, predator meter and
  original cocoon-related HUD variables. The original countdown/time source
  (`counttime`, `counting`, `ticktime`, `flash_dir`, `frames_now`, `one_sec`)
  is captured independently as WORLD-owned state, not advanced once per
  player. Original HUD display/audio callback functions remain host-owned.
- `new/multiplayer/local_mp_actor_native.h/.c`: HUD message and player-HUD
  visual state are now inside the same original-engine transaction as
  PLAYER.S and MAZESCRN.S. Prevalidation occurs before changing host globals,
  and the scope restores the original host HUD state on leave.
- `new/multiplayer/local_mp_native_context.h/.c`: the world transaction binds
  and restores its independent countdown and HUD clock along with original
  doors, AMP and collision state. Shared worlds use ONE set of countdown
  state, irrespective of how many players occupy them.
- `CMakeLists.txt`: both the standalone original-game library and the actual
  PC executable's core receive the feature-gated source hooks. Existing
  multiplayer native tests now link the real original HUD units.

## Regression results

- GCC and Clang strict original PC executable builds: PASS.
- Four original player/weapon/HUD contexts: PASS. Per-player pickup notices,
  access-code text, destination text, countdown display text, tracker values,
  map state and cocoon records are independent, with complete host rollback.
- Forged HUD magic rejection: PASS before any mutation of the original host.
- Two level-owned countdown clocks and original pad counters stay distinct,
  and the host state is restored: PASS.
- Native actor/world integration and 4-frame session compositor: PASS.
- GCC: 15 relevant tests and one-window X11 pixel readback PASS (synthetic
  test frames ONLY). Clang native focused tests PASS. Focused ASan/UBSan tests
  PASS. Strict multiplayer-disabled PC executable PASS.
- Windows compilation/runtime, physical multiple mice, and actual four live
  game-world frames: NOT TESTED.

## Critical limitations: NOT playable multiplayer

- `pc_main.c` has NOT been switched to a multiplayer game scheduler. The
  original retail level/player initializer and real renderer need to be
  integrated with the session's create, join, advance and draw callbacks.
- The HUD is not wholly independent yet: original GPU presentation state,
  callback-painted HUD brush/screen buffers, audio voice handles, campaign
  countdown semantics across different worlds and player-specific terminal/
  automap interactions still require the correct ownership and real runtime
  tests. This sweep only captures the original CPU HUD variables listed.
- `UpdateAMPs()` still targets a single original player global in many
  routines. Multi-target AI, combat, player collision and projectile effects,
  shared-level interactions, full mixed-species campaign save/restore, and
  Windows Raw Input hardware routing remain unfinished.
- Do not enable Start Co-op until four actual interactive player views are
  running and tested using original Jaguar assets and behavior.
- No original retail ROM, Jag SDK materials, external emulator code, fake
  sprites, worker processes, network implementation, or extra windows added.

## Reproduce native tests

```
cmake -S . -B build-gcc -G Ninja -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-gcc --target avp_pc test_local_mp_actor_native test_local_mp_native_context test_local_mp_native_backend -j6
build-gcc/new/multiplayer/test_local_mp_actor_native
build-gcc/new/multiplayer/test_local_mp_native_context
build-gcc/new/multiplayer/test_local_mp_native_backend
```
