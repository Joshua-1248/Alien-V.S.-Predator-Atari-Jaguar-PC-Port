# Atari Jaguar Alien vs Predator — RE #26 / S62

Baseline: RE26 S61 complete source. Optional local cooperative extension only.

## Actual implementation changes

1. **Split the actual original `PLAYER.S` implementation, not a replacement.**
   `UpdatePlayer()` still calls `readpad();` then the exact source player
   update body, `MoveDoors(); BGSounds();` in its original order. One common
   `update_player_actor_body()` avoids duplicate mechanics. The optional
   `avp_mp_player_native_step_actor()` calls that SAME original body with the
   session's prepared actor-specific `joy_cur` and `joy_edge` and its own
   original background-sound RNG. `avp_mp_player_native_step_world_doors()`
   advances original `MoveDoors` exactly once for a shared level. The original
   actor-specific `DoorKeys()` stays inside each player's update; timed door
   progression does not run per player.
2. **Correct original frame counter ownership.** The original host `readpad()`
   increments `frame_counter` once per single-player tick. The secondary
   player path must NOT call `readpad()` (it would overwrite P2/P3/P4's pad
   with P1). The native world coordinator now binds the per-world original
   `frame_counter`, increments it once per world step BEFORE actor updates,
   captures its new value and restores the host counter. Render-only visits
   leave the counter unchanged. This matters for the original pain-sound
   cooldown and future frame-dependent actor effects.
3. **Actual native Jaguar-derived renderer entry point.** The existing
   `pc_video_x11.c` now exposes optional `avp_pc_video_local_mp_render_actor`.
   While the intended original actor+world state is bound, this function calls
   the game's REAL `avp_gpu_render_maze` with that actor's camera, the selected
   level's original maze, original AMP sprite state, weapon overlay and HUD.
   Each player gets its own independent 320x228 XRGB framebuffer, occlusion
   scratch and mutable renderer state. Immutable ROM-derived images are
   borrowed from the live GPU asset store; assets are NOT copied or generated.
   Borrowed GPU states are explicitly released before the source assets are
   reloaded or destroyed. `avp_pc_video_local_mp_snapshot_actor()` provides
   per-view original presentation history for future smooth presentation.
4. Everything remains in one executable/game window. With the optional
   extension disabled, S62-specific APIs/symbols are excluded. Original
   single-player algorithms, ROM assets and AI code are unchanged.

## Tests performed

- GCC and Clang strict builds of actual `avp_pc`: **PASS**.
- Focused original player tick regression, original normal `UpdatePlayer`
  compared against co-op player+door split, three species plus locked input,
  original movement, independently directed two actors, host rollback: **PASS**.
- Native backend and state tests: **PASS** with two world clocks isolated.
- Previously shipped 4-player input, gamepad, world, AMP, weapon, door,
  compositor and retail-map tests: **PASS** (GCC).
- Clang focused native+input regression suite: **PASS**.
- One real X11 game window, 1-4 compositor pixel readback: **PASS** on GCC
  and Clang, with **synthetic test pixels**.
- Multiplayer-OFF strict PC binary excludes new co-op symbols: **PASS**.
- Address/undefined behavior sanitizers on original tick and native state
  tests: **PASS**.
- The real GPU render function above is compiled into `avp_pc`, but its
  visual correctness in four live scoped worlds is **not runtime verified**.
  No retail Jaguar ROM is packaged. Windows compilation and runtime are **not
  tested** in this environment.

## Still blocking a playable co-op release

- Install a real session backend in `pc_main.c`: instantiate each player with
  original initializers/retail inventory, and each world with original
  `InitAMPs`/door/resource bindings. Connect the 10-Hz update loop.
- Scope other mutable original gameplay globals and HUD message/automap state
  to avoid stale or leaked player-specific state in the real renderer. Audio
  voices, save state, and terminal interaction also need ownership rules.
- Update original `UpdateAMPs` AI to correctly select and attack all live
  player actors sharing a level; run original AI only once per world tick.
- Implement original collision/projectiles/death/respawn/level travel between
  independently controllable actors, and mixed-species campaign progression.
- Route Windows raw multi-mouse events (WM_INPUT) and test real multi-USB-
  mouse/gamepad hardware on Linux and Windows.
- Do not enable Start Co-op until all live movement, combat, rendering and
  campaign regressions are working. No process spawning, network play,
  simulated sprites or quicksave state-swapping are permissible.

## GCC build

```sh
cmake -S . -B build-s62-gcc -G Ninja -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-s62-gcc --target avp_pc test_local_mp_player_step test_local_mp_native_backend test_local_mp_native_context -j6
build-s62-gcc/new/multiplayer/test_local_mp_player_step
build-s62-gcc/new/multiplayer/test_local_mp_native_backend
build-s62-gcc/new/multiplayer/test_local_mp_native_context
```

The Jaguar retail cartridge, emulator internals and nonredistributable assets
are NOT included. Original Jaguar source/ROM remain authoritative.
