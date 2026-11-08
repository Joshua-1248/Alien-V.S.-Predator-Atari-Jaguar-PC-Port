# RE26 / Sweep 65 — live original-engine two-player movement/view test

Base: complete Sweep 64 original/PC source. This is an **opt-in, experimental WALKTEST**, not working campaign co-op and not an alternative game engine.

## Actual integration

`src/platform/pc/pc_main.c` has a `--mp-walktest-2` branch **after** an original-level campaign/diagnostic initialization. The alternate 10 Hz loop runs `AvpMpSession` through the existing native-world/actor transactions and presents two independently rasterized original-engine camera frames inside **the existing single PC window**. Normal gameplay never enters this branch. A menu click alone does not launch co-op.

`new/multiplayer/local_mp_walktest.c` initializes the two player states from the original, already initialized actor. Only a same-species P1/P2 pair and one already loaded level are supported. P2 obtains an independent interior location checked using the actual original `SafePos(80, ..., 40)`; it does not use an invented collision routine. If the map has no valid second location, startup fails rather than reusing the P1 position. P1 continues to use the legacy PC key/pad mapper; P2 uses its independently configured bindings.

Each original actor step first publishes original MAZE.S fixed-point `sin_ang` / `cos_ang` for that actor's authoritative `centre_angle`, then executes the same translated PLAYER.S movement/keys/sound body. Without these original pre-movement vectors, both input streams arrived correctly but **neither player moved**; that defect was reproduced and fixed. Timed original doors tick once per shared-world update, not once per player.

The active-level factory `avp_mp_native_capture_active_world()` captures the **original loaded world's** AMP allocator/actor grids, complete door machine (including panel, access and level metadata), HUD world clock and pad frame counter. It verifies a leased private copy of the correct decoded retail level, copies the *currently mutated* maze cells only after successful validation, remaps the original door offsets to this private copy, and leaves host state untouched. The previous S65 draft took `source - 8` as a retail file header, an unsafe assumption for arbitrary runtime maze bindings; now the live launcher passes the actual `AvpMazeCandidate` returned by the retail FILES loader. The no-ROM developer geometry is clearly marked and supplied with its own diagnostic header only.

## Verification on Linux

- Strict GCC `avp_pc` and `test_local_mp_native_context`: **PASS**.
- Strict Clang `avp_pc`, native context/backend/player-step integration tests: **PASS**.
- Strict GCC **16** local-MP test executables including X11 **single-window real pixel readback** under Xvfb: **PASS**.
- Clang ASan+UBSan original-native context/backend/player-step tests: **PASS** (`detect_leaks=0`).
- Strict `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` `avp_pc` build: **PASS**.
- One real OS process and one Xvfb/X11 window diagnostic live boot, **60+ original 10 Hz ticks**, independent P1 and P2 positions: **PASS**. A separate 26-tick run closed the actual X11 window via `WM_DELETE_WINDOW` and the program exited normally with status 0 and `fault=0`. A test keyboard script held P2's independently bound `i` while P1 was neutral; P2 changed x position and P1 did not. After release it pressed P1's ordinary `w`; P1 moved using the unchanged legacy binding. P2 continued to obey the source's inertia/collision, not the P1 input.
- The no-ROM diagnostic capture was mostly blank, **not** verified visual parity. Retail game asset renderer/video output with the actual copyrighted Jaguar ROM, physical multiple mice, and Windows runtime: **NOT tested here**. No user ROM is included or distributed.

## How to run this isolated test

From a Linux build, place your own original `AVP.jag` beside `avp_pc` (or pass `--rom /path/to/AVP.jag`) and run:

```sh
./avp_pc --rom /path/to/AVP.jag --mp-walktest-2
```

In F12 → Controls → Local Players, assign Player 2 independent keys or controller/mouse, or add e.g. `local_multiplayer.input.p2.key_0=0x69` to `settings.cfg` beside the executable (`i` is Forward for P2). Player 1 retains existing keyboard/gamepad bindings. `./avp_pc --diagnostic --mp-walktest-2` runs a developer geometry smoke *without retail assets* and should not be judged by its visuals. Close the window to exit.

## Not yet safe to call finished or enable Start Co-op

- Full original enemy/hostile targeting across multiple species is unresolved; the walktest intentionally does NOT tick AI, instead of applying damage to whichever player was last bound. Combat interactions are **not validated**.
- Level transfers, terminals/computers, death/respawn, mixed-species initialization/inventory, campaign completion, persistent co-op saves, player-vs-player/creature collision, per-player voice ownership, physical multi-mouse Win32 WM_INPUT, and retail ROM graphical fidelity remain incomplete. Terminal/death/exit requests currently fault/end the walktest rather than execute invalid transitions.
- The original HUD/weapon updates in the live walktest do not yet reproduce the complete NextFrame/PreFrame/SwapScreens/PostFrame/UpdtHUD pipeline. It is a **movement/view test**, not full HUD/combat parity.
- The original hardware game does not define a second-player spawn; nearest source-collision-valid interior cell is an explicit *new co-op design choice*, not asserted as an original Jaguar rule.

**Next goal:** run the retail-ROM walktest with 2 independent inputs and inspect *two original rendered camera+HUD frames*. Then complete shared target/weapon and AMP life-cycle behavior and route the actual NextFrame/HUD/audio stages through the session before enabling campaign co-op.
