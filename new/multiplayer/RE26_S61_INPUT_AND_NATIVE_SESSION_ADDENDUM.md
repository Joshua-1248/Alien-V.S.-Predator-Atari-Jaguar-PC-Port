# AVP Jaguar PC reconstruction — RE #26 follow-up (S61)

Base: the previously verified RE26 native-state integration derived from RE25
Full Sweep 59. This change is an **optional multiplayer extension**; it does not
modify the original Jaguar single-player simulation or enable playable co-op.

## Implemented

1. `local_mp_commands.[ch]` converts separately owned held X11/Win32 keyboard
   KeySyms, raw physical mouse button/motion samples, and identified controller
   button/analog states into up to four distinct `AvpMpPlayerCommand` values.
   Mouse delta is consumed once per tick; pressed buttons remain held. P1's
   original legacy PC key/controller state is never implicitly overridden.
   Conflicting gamepad paths and invalid configurations reject the complete
   command frame, without publishing a partial result.
2. `local_mp_gamepads.[ch]` polls independent Linux `js_event` devices through
   nonblocking `/dev/input/js*` descriptors. Windows XInput slots `xinput:0` ...
   `xinput:3` are represented through independent slot polls. Linux disconnection
   clears cached buttons and axes. The original P1 joystick remains separate.
3. `pc_video_x11.c` implements optional
   `avp_pc_video_local_mp_take_commands()`. It reads held keyboard KeySyms from
   the existing host keyboard state; gathers identified evdev mouse motion;
   polls P2-P4 gamepads; checks for conflicts with the existing P1 pad; and
   clears cached samples on inactive/title/menu/unfocused state. This entry is
   **not yet called by the game's live multiplayer simulation loop**.
4. `local_mp_native_backend.[ch]` connects the existing `AvpMpSession` factory,
   join, level tick, player tick and render callback contracts to original
   Jaguar-derived per-player weapon+player snapshots and original-door/AMP/
   collision world-state snapshots. A mandatory P1 input callback preserves
   its legacy mapping; P2-P4 use `avp_mp_native_input_prepare`. Original game
   actor callbacks run once per player, then world callbacks once per world.
   Runtime callbacks and initial states must still be supplied by a future
   production gameplay integration. No fake actors, physics or AI are installed.
5. `local_mp_input.c` validates gamepad ownership and prevents a physical
   controller being assigned twice, including Linux device path aliases.
   The existing input menu and profile persistence are preserved.

## Verification

- Strict Linux GCC and Clang `avp_pc` production builds: PASS.
- Native backend session test with original reconstructed C player, weapon,
  AMP, door and collision state: PASS, GCC and Clang. Exercises four actors,
  two levels, same-level world ticks once, level travel, distinct player
  frame buffers, and exact restoration of host state. The frame contents
  are **synthetic test data**, not original gameplay rendering.
- Independent input command test (held actions, mouse consumption,
  directional stick thresholds, inversion, invalid config rejection): PASS.
- Linux joystick event input tests: PASS, GCC and Clang. Test uses a pipe as
  the event source, NOT a physical USB controller. Tests init events, button
  state persistence, release, axis change, EOF/disconnect clearing.
- Previous local-world, compositor, original actor/weapon/player/AMP/door,
  and one-window X11 pixel-readback tests: PASS, GCC and Clang.
- `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` strict PC build: PASS.
- AddressSanitizer + UndefinedBehaviorSanitizer native backend and input
  command integration tests: PASS.
- **Not tested:** Windows compilation or runtime, multi-USB-mouse hardware,
  physical multi-joystick gameplay, retail ROM frame parity on new bridge.

## Significant remaining work before real co-op

- Install a real `AvpMpNativeBackendHost` from the original PC game main loop,
  including original retail player/level initialization and transitions.
- Feed each player an authentic native Jaguar 10Hz simulation step, not merely
  input setup. Correctly isolate all additional mutable globals referenced by
  `MovePlayer`, `UpdatePlayer`, `UpdateAMPs`, native AI and combat, and finish
  mapping shared-level actors. Shared enemies/doors must tick once per world.
- Render original `MAZESCRN`, original HUD and character sprites from each
  true actor/camera into separate owned framebuffers, then call the already
  implemented compositor and *one-window* presenter. Currently the integration
  test supplies distinct synthetic pixels solely to test the contract.
- Distinguish the X11 aggregate/legacy mouse from dedicated evdev mice so raw
  movements are not double-counted. Implement actual Windows `WM_INPUT`
  physical multi-mouse event delivery, not just device enumeration.
- Bind audio ownership/mixing, shared-level AI targeting, deaths, weapons,
  inventory, level travel, character-specific campaign scripting and saves.
- Cross-compile Windows and test physical hardware on Linux + Windows.
- Keep **Start Co-op** disabled until the native runtime produces playable,
  independently controllable original-rendered frames and regressions pass.

The project uses ONE process and ONE window. Do not introduce isolated Linux
worker processes, networked simulation, substituted player AI, altered source
mechanics, or runtime quicksave swaps. Original Jaguar ROM/source/hardware are
fidelity references. BigPEmu is allowed as a black-box behavior reference only.

## Linux reproducibility

```sh
cmake -S . -B build-gcc -G Ninja -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-gcc --target avp_pc test_local_mp_commands \
  test_local_mp_gamepads test_local_mp_native_backend test_local_mp_native_context \
  test_local_multiplayer_input test_local_multiplayer_worlds \
  test_local_multiplayer_compositor test_local_mp_one_window_x11 \
  test_local_mp_actor_native test_local_mp_weapon_native \
  test_local_mp_amp_native test_local_mp_doors_native test_local_mp_player_native -j4
build-gcc/new/multiplayer/test_local_mp_commands
build-gcc/new/multiplayer/test_local_mp_gamepads
build-gcc/new/multiplayer/test_local_mp_native_backend
xvfb-run -a build-gcc/new/multiplayer/test_local_mp_one_window_x11

cmake -S . -B build-clang -G Ninja -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-clang --target avp_pc test_local_mp_native_backend \
  test_local_mp_commands test_local_mp_gamepads -j4

cmake -S . -B build-off -G Ninja -DAVP_STRICT=ON \
  -DAVP_BUILD_LOCAL_MULTIPLAYER=OFF -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-off --target avp_pc -j4
```

Original retail ROM and third-party proprietary emulator internals are NOT
included in this package.
