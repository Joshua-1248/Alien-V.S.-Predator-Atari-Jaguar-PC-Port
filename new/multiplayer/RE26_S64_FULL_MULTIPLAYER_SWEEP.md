# Alien vs Predator (Atari Jaguar) — RE #26, Full Sweep 64

Baseline: RE26/S63 complete source archive, not a reconstruction from notes.
Goal: locate real co-op launch blockers; advance native original-AMP targeting without altering the original single-player Jaguar AI code. **Multiplayer remains non-playable.**

## Source-level sweep: what is present and what is still absent

| Subsystem | Inspected source | Established status and remaining gate |
|---|---|---|
| Live game loop | `src/platform/pc/pc_main.c` | `UpdatePlayer`, `UpdateAMPs`, HUD, presentation, terminal, saves and level transfers still run in the original single-player `while(!quit)` loop. It never creates or ticks `AvpMpSession`. A live co-op path is **not** implemented. Do not enable the F2 Start Game option. |
| Session ownership | `new/multiplayer/local_mp_worlds.c` and `local_mp_native_backend.c` | Four original actor contexts; one world per distinct active level; join/leave and unique-frame composition established. The native backend is now also able to dispatch each AMP under an explicitly selected original actor context within its shared-world step. The caller must supply real initialization, resources and gameplay callbacks; currently tests do this with isolated fixtures. |
| Original player movement | `src/game/player.c`, `local_mp_player_native.*` | Original `UpdatePlayer()` untouched; optional actor-step shares its movement/keys body without reading P1's pad; original timed door advance may be done once per world. Still requires real gameplay tick integration and player-vs-player/actor collision rules. |
| Enemy and projectile AI | `src/game/amp.c`, `local_mp_amp_targets.*` | `UpdateAMPs()` reads singleton `x_pos`, `y_pos`, `player_type`, `player_energy` throughout its creature modes. Calling it once with a leftover player global is **not** multiplayer. Sweep 64 adds a guarded per-AMP original-mode target transaction, preserves original loop order/timers, and dispatches through the native session on explicit opt-in. **Enemy-specific mixed-species target-selection policy, weapon combat, targeting multiple candidates and full lifecycle parity are NOT completed.** |
| World/door/AMP storage | `local_mp_native_context.c`, `local_mp_amp_native.*`, `local_mp_doors_native.*`, `local_mp_retail_maps.*` | Scoped world binding, original grids, allocator, door timers, saved placements and retail map copies; world clock once/level. AMP external template/callback/resource bindings and audio voice ownership remain global. |
| HUD/weapon state | `local_mp_actor_native.*`, `local_mp_weapon_native.*`, `local_mp_hudmsg_native.*`, `local_mp_hudview_native.*` | Independent original weapon scripts, HUD messages, first-person HUD state; actor validity now checks **all** four ownership structures at creation. Terminal/computer and full original renderer/global display ownership are not fully isolated. |
| Rendering | `src/platform/pc/pc_video_x11.c`, `local_mp_compositor.c` | One-window four-viewport compositor tested via X11 pixel readback; real Jaguar-derived per-actor rendering callback compiled. Actual four-player moving/original-HUD video from independently simulated actors has **not** been verified live. |
| Keyboard/gamepad/mice | `local_mp_commands.c`, `local_mp_gamepads.c`, `local_mp_mice.c`, `local_mp_native_input.c` | Separate input collections, physical mouse IDs on Linux, Linux joystick and Windows XInput reader. Windows raw-device **enumeration does not equal WM_INPUT delivery**: the per-device Windows mouse event handler is missing. Live device tests on Windows/Linux not done. |
| Mixed-species campaigns | `local_mp.c`, `local_mp_worlds.c`, `pc_main.c` | Session permits different player species while following one selected campaign. Live level-specific initialization, persistent corpses/doors, transitions, player death and victory/game-over ownership and per-player saves/terminals remain unimplemented in co-op. |
| Audio | `pc_audio.c`, original AMP/PLAYER/HUD callers | Original audio effects are global host voices. AMP loops and warning alarms need world/player source ownership and shared spatial mix; multi-character combat audio not proven. |
| Single-player parity | Original `src/game/amp.c` diff, `src/game/player.c` | **AMP.S normal single-player loop unchanged byte-for-byte**; only an optional end-of-file include added. No original AI mode, physics, ROM rendering, levels or single-player branch rewritten in this sweep. |

## S64 changes implemented

1. New `local_mp_amp_targets.c/.h/.inc`: bounded roster of up to four valid, independent original Jaguar actor contexts. For each active original AMP, a host-supplied policy selects a live roster member. The original `amp.c` mode function runs **exactly once** while that player's original globals are bound; the selected player's changes are captured, the ordinary host state is restored, and the shared AMP continues to the next entry. Original AMP loop prologue, `entime` old-energy behavior, frozen-NPC filter, `cur_amp_no`, and loop-handle expirations are preserved.
2. Added world/actor scope-activity checks. The multi-AMP function rejects calls outside an active original-world scope or inside an existing actor scope; invalid actors, masks, or missing selection callbacks are refused. A rejected target selection faults the tick instead of silently targeting P1 or reusing the last player.
3. `local_mp_native_backend.c/.h` accepts optional `choose_amp_target`. Its world step may run the original AMP targeted loop and then the host's **other** world work. **When enabled, `advance_level` MUST NOT call `UpdateAMPs` a second time.** Without a target callback, pre-S64 behavior remains unchanged.
4. `avp_mp_actor_native_valid()` now validates the whole actor (player, weapon, HUD messages, HUD view) before source-level creation or multi-AMP activation.
5. Augmented real-original-engine `test_local_mp_native_backend.c`: two distinct actor snapshots on a shared map; only designated P2 receives a source-mode mutation, P1 unchanged; one AMP executes once; original host state fully restored; invalid roster and target choice rejected; **byte-for-byte** original `UpdateAMPs` comparison on a controlled single-target original AMP mode test fixture; optional native session scheduler executes one more AMP tick, once/world. Mode callback used in the fixture is test-only to observe targeting; it is NOT a replacement combat AI in the shipped game.

## Verification (this environment)

- Strict GCC `avp_pc` executable: PASS.
- Strict Clang `avp_pc` executable: PASS.
- GCC all **15** local multiplayer regression executables: PASS.
- GCC X11 single OS window / 1–4 viewports / HUD row pixel readback under Xvfb: PASS. The display patterns in that test remain synthetic, not proof of live co-op.
- Clang targeted backend, original actor/world/AMP/player tick, and multiworld tests: PASS.
- Clang AddressSanitizer + UndefinedBehaviorSanitizer focused original actor/native world/native backend: PASS (leak detection disabled for the test run).
- Strict `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` `avp_pc` executable: PASS. New API excluded.
- `avp_pc --help` smoke: PASS.
- Windows MinGW toolchain and actual multi-mouse hardware not available here; Windows build/runtime NOT verified.
- User-owned retail AVP.jag was not available in this environment for this sweep. **No ROM parity claim or live co-op claim.**

## Next verified milestones, in dependency order

A. Implement a **source/ROM-grounded multi-species AI target policy** and verify representative real AMP modes (Marine, Alien, Predator, facehugger, Queen, projectiles, generators, scripted transitions). Ensure creatures only tick once/level; apply damage to the correct player context; preserve original one-player behavior and original RNG sequencing. The optional target dispatcher makes this possible but does not invent the policy.

B. Build real original actor/world factory callbacks from retail map, AMP placements, door and level data. Define source-correct per-player initial inventory, species, spawn, level persistence and inter-player collision. Then connect those validated factories to a separate optional co-op path in `pc_main.c` at the original 10 Hz, without changing the single-player loop.

C. Render each real player into their own retained original 320x228 full-HUD frame with the original rasterizer and call the **existing one-window** composer/presenter. Prove two real moving views, then three/four. Preserve original viewport HUD content and scaling rules.

D. Route real independent input into the live co-op scheduler, including Windows WM_INPUT device-specific mouse motion. Verify with physical multiple mice and gamepads; ensure Player 1 legacy input remains unchanged.

E. Isolate original combat/audio, terminal, death/respawn, savegame, class-specific world resource, per-level transitions and shared campaign completion; validate Linux and Windows co-op end-to-end before enabling Start Co-op. Do not claim complete until these work.

**No Linux-only processes, separate windows, networking, dummy creature AI, synthetic player sprites, substitute gameplay, or external emulator internals added.**
