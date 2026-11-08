# Alien vs Predator Jaguar PC port — RE #26 native multiplayer integration

**Baseline:** RE #25, Full Sweep 59 complete source.
**Status:** Compiles and integrated native state tests pass; **playable co-op is NOT yet implemented.**
**Important:** This is not a placeholder game simulation. There is no new player AI,
new physics engine, invented assets, duplicated first-person frame, or separate OS window.

## What changed (all under `new/multiplayer/`)

- `local_mp_native_context.{h,c}`: a single gameplay-thread transaction tying the **original** AMP allocator/world records, **original** DOORS machine, original COLLIDE.S maze binding, host maze dimensions and one coherent PLAYER.S+MAZESCRN.S actor together. Each actor enters/leaves independently; the shared world callback is executed **once after all participating actors**. Original host globals and bindings are restored on normal exit. A rejected nested world or actor visit does not run the callback.
- `local_mp_native_input.{h,c}`: translates a per-player action bitmask into the original Jaguar **active-low** pad, including original rising-edge rule, Alien/other-species turn modifiers, original Alien attack keys, Predator cloak/vision/medkit, moving/using/map/pause/option, simultaneous sidestep-through-turn and the existing PC mouse impulse math with per-player sensitivity. No global P1 input changes. This adapter does not poll hardware.
- `tests/test_local_mp_native_context.c`: links and executes the original `player.c`, `weapons.c`, `mazescrn.c`, `amp.c`, `doors.c` and `collision.c` in one process. Explicit original amplifier allocator, `SafePos`, `SaveDoors`, and weapon script state are exercised over **two different level contexts / four independent player actor states**. Tests inspect host restoration and prevention of double world updates, native pad edge semantics, mixed-species turn bindings, mouse impulse and rejected bad contexts. Test stubs cover only unrelated hardware/audio function outputs; no fake AI, movement, or collision are substituted.
- Original Jaguar game units under `src/game/` are **unchanged** by RE26. Multiplayer `OFF` still completely excludes these modules from `avp_pc`.

## Verified

- Native strict GCC/Clang Release builds of `avp_pc` and the integrated regression.
- Original combined player+weapon, AMP and door tests and multiplayer input/worlds/compositor tests under both compilers.
- Strict GCC `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` PC executable.
- See `RE26_BUILD_VERIFICATION.txt` delivered next to the patch for exact commands and results.

## What remains to make co-op playable (do not misreport as done)

- The runtime still calls the single-player original `UpdatePlayer()` (which invokes `readpad()` and `MoveDoors()` inside the player step). The new native coordinator is not called from the PC main gameplay loop; this must be separated without changing single-player's original behavior.
- Original AI targets and collision/attack resolution still assume one current original-game player. Cross-species/co-op target selection, friendly actor hits, per-player interactions, and once-per-world AI update are **not live**.
- Shared world snapshots currently isolate DOORS/AMP/maze collision, **not the complete** original level resources, campaign/global HUD, audio voice owner tables, per-player renderer camera, automap, inventory/level transitions/save states or terminal/duct presentation.
- No independently rendered original 3D+HUD frames or original-player body sprites have been piped to the existing single-window compositor. Test compositor frames are synthetic and never claimed to show gameplay.
- Separate live keyboard, gamepad, physical Linux mice and Windows Raw Input are not yet polled/connected to distinct player actor scopes. The input adapter does not yet handle weapon cycling, host Quick Turn, terminals, vertical mouse view or arbitrary gamepad analog values; these require their original PC host-side control paths.
- No live 1–4-player gameplay testing or Windows cross-compile/runtime validation has been performed.

## Native callback rules

1. All calls must be on the game thread, non-reentrant, with a zeroed scope.
2. The `AvpMpNativeWorld` map pointer must be a currently leased real decoded original map; its header/dimensions/level must match the bound original DOORS snapshot.
3. The caller must supply original-engine initialized player and world snapshots (e.g. from a validated actual retail loader). DO NOT synthesize them from unknown ROM layouts or copy quicksave blobs as runtime state.
4. Actor callback advances one original player/weapon owner; world callback advances original world systems **once** for all participating actors. The callbacks must not switch worlds or reenter the bridge.
5. A callback failure is **non-transactional**: the mutated contexts are retained, and the owning session must fault instead of replaying the tick. This bridge only restores the original host state, not external audio/render/terminal side effects.
6. `avp_mp_native_input_prepare` maps supported source button actions and horizontal mouse values; unhandled host-only actions need separate original PC host dispatch. Do not mistake an input translation test for hardware input delivery.

## Build

```sh
cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-gcc --target avp_pc test_local_mp_native_context
build-gcc/new/multiplayer/test_local_mp_native_context

cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-clang --target avp_pc test_local_mp_native_context
build-clang/new/multiplayer/test_local_mp_native_context

cmake -S . -B build-off -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_LOCAL_MULTIPLAYER=OFF -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-off --target avp_pc
```
