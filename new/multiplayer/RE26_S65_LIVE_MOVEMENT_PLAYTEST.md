# RE26 / Sweep 65 — First live original-player two-view integration

## New opt-in test, NOT completed co-op

`avp_pc --rom /path/to/your/retail/AVP.jag --mp-walktest-2`

This runs a **two-player same-species movement/view test** in the one existing PC executable and one existing OS window. The original campaign initializer first loads the selected game and level. The test captures that initialized original player state, original AMP/door/world HUD state and a private copy of the actual decoded level, then creates two independently owned original player contexts. It uses the original PLAYER.S-derived movement routine (not invented player physics) at 10 Hz. Each player's original position and heading is rendered by the existing Jaguar-derived software renderer into an independent framebuffer; the existing viewport compositor presents both in the same OS window. The player-state bridge restores the host's original singleton globals after every world visit. Each world's original timed doors are advanced once per tick, not once per player.

For a non-retail **technical diagnostic only**, run `avp_pc --diagnostic --mp-walktest-2`; the game's pre-existing diagnostic maze is not a Jaguar retail map. It cannot prove any retail gameplay fidelity. The retail ROM is not included.

**Input setup:** P1 uses the existing normal PC keyboard/gamepad bindings. P2 uses the existing F12 → Controls → Local Players → Player 2 configuration, saved to `settings.cfg` next to the binary. Configure that before starting the opt-in test. For a temporary keyboard example if no physical second device is available, edit only these existing settings keys in `settings.cfg`: `local_multiplayer.input.p2.key_0=105` (forward: I), `key_1=107` (back: K), `key_2=106` (turn left: J), `key_3=108` (turn right: L). If you edit the file, close the game first. Both separate player inputs were exercised using X11 test key events on Linux.

**Intentional hard limitations:** Same species as the loaded campaign actor for both players; two players only; current level only; P2's additional spawn is found using the original `SafePos` collision test, not an authored co-op spawn. Native AMP/enemy AI is NOT run because the mixed-species/co-op target policy is unfinished. Combat ownership, HUD refresh/clock and per-player camera pitch, audio ownership, inter-player collision, object/AMP pickup interactions, death handling, inventory persistence, original single-player pause/terminal handling and campaign level transitions are not finished or verified. The experimental session stops on unsupported transitions and faults. **The F2 Start Co-op action remains disabled**. This is for checking independent real player movement, cameras and video, not for playing the campaign cooperatively.

The `--mp-walktest-2` flag exists only when CMake `AVP_BUILD_LOCAL_MULTIPLAYER=ON`; it is not enabled in an ordinary game launch. When the multiplayer extension is disabled at build time, the normal single-player executable still compiles and does not expose or start the test.

## What was verified in this environment

- Strict GCC and Clang Linux `avp_pc` builds: PASS.
- Strict build with `AVP_BUILD_LOCAL_MULTIPLAYER=OFF`: PASS.
- 16 original multiplayer regression executables: PASS.
- Original single-window X11 compositor pixel-readback: PASS.
- Headless Xvfb live **diagnostic-maze** movement/view path: PASS. Completed more than 60 original 10 Hz ticks, drawing to the same OS window without a fatal multiplayer session fault. The headless run was terminated externally after checking continued progress.
- X11 input injection: P2's independently assigned Forward (`i`) moved only P2 while P1 stayed still. P1's normal Forward (`w`) then moved P1 under the original motion rules. The original MAZE.S pre-movement sin/cos update was necessary for this to work; without it both input streams arrived but produced no motion.
- Clang AddressSanitizer/UndefinedBehaviorSanitizer native original-player/world/session unit tests: PASS. Full live-runtime sanitizer execution is not asserted here.
- A real `WM_DELETE_WINDOW` event cleanly ended the experimental walktest, restored the normal PC host shutdown and exited with status 0, no reported session fault (26 simulated ticks).
- **NOT VERIFIED:** a loaded retail AVP.jag is not supplied in this environment, so the opt-in path has not been exercised with actual game resources; Windows executable/runtime and multiple physical mice/gamepads were not available to test.

## Development priorities after S65

1. Connect and validate a source-grounded co-op enemy targeting policy, enemy/projectile collisions and combat ownership. Keep a single AI update per world.
2. Stabilize per-player original HUD redraw, first-person pitch and effects on two independently rendered live views.
3. Implement authored world transitions, independent species starts/inventory and saved state with strict retail fidelity.
4. Complete native Windows WM_INPUT multi-mouse routing; test physical hardware and multiplayer audio.
5. Expand to 3–4 players only after simultaneous same-world two-player combat/level transitions are stable. Preserve 1 executable / 1 window and ordinary single-player behavior.
