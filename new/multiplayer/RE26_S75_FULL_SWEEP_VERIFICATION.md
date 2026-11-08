# Alien vs Predator (Atari Jaguar) — RE #26, Sweep 75 full verification

**Baseline:** RE26/S74 user source. **Retail oracle:** user's original 4,194,304-byte Jaguar AVP ROM (SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`). Neither the ROM nor BigPEmu binary/code is included in any release ZIP.

**Scope:** Full sweep of native PC co-op source and reproducible regressions, Linux retail-ROM F2 gameplay, player counts/layouts, keyboard/mouse binding, actor/world isolation, player equipment/HUD/map, render/sprites, audio event ownership, life/transition policies, and single-player fallback. No unverified claims of complete co-op are made.

## Changes from Sweep 74 (three source files, two new test scripts)

1. `new/multiplayer/local_mp_walktest.c`: When multiplayer command capture rejects a tick (F12/F2/FocusOut etc.), also clear P1's **legacy host analog forward/strafe/turn/aim**, keyboard strafe and queued weapon delta, plus prior secondary weapon buttons. Previously only releasing P1's emulated Jaguar pad and P2–P4 commands left a path for P1 stick axes to continue affecting gameplay behind a blocked-input menu. The original single-player input pipeline is untouched. This is source-verified and compiles; physical joystick focus testing remains necessary.
2. `src/platform/pc/pc_video_x11.c`: Repair P2–P4 F12 keyboard capture so a `KeyRelease` received during `capture_mode=6` clears the held-key table. Without this, **Enter can remain held**, preventing subsequent Enter-to-bind operations. Repeated capture is now proven in an actual F12 menu X11 test.
3. `new/multiplayer/tests/re26_retail_multi_view_smoke.py`: Update an outdated test assertion. Since S73 renders real moving player sprites in *other* players' views, the old assertion that an unmoved viewport be pixel-identical is false. New test reads actual per-player original fixed-point positions/heading from engine logs and verifies untouched actors do not move while the commanded actor does.
4. New reusable `new/multiplayer/tests/re26_s75_native_regressions.py`: Explicitly runs all **22** native test programs, with isolated Xvfb and a user-supplied original ROM for the retail test. Important: this tree does **not** register tests with CTest; `ctest` alone reports *No tests were found* and cannot be considered a pass.
5. New `new/multiplayer/tests/re26_s75_bindings_x11.py`: Real F12 menu navigation/XTEST regression for sequential **P2, P3, P4 keyboard binding** and **P2 mouse-button capture**, verifying they persist to the correct settings. This uses virtual X11 inputs only; physical USB device routing remains untested.

## Build and test matrix

| Check | Result and qualification |
|---|---|
| Clean strict GCC Linux `avp_pc`, Multiplayer ON | PASS, Release |
| Native multiplayer test executables | PASS, **22/22**, including source actor/weapon/HUD state, collision, AMP, doors, respawn policy, world transactions, audio-independent input, original retail resource audit, original movement, compositor and X11 single-window |
| Strict Clang Linux `avp_pc` | PASS |
| Multiplayer-disabled strict GCC `avp_pc` | PASS |
| ASan/UBSan production + focused tests | Build PASS; **8/8** focused tests PASS with `ASAN_OPTIONS=detect_leaks=0` and UBSan halting on errors |
| Sanitizer toolchain warning note | GCC `-O1 -Werror` alone flagged unrelated pre-existing `pc_main.c::logmsg()` as `-Wformat-overflow`; sanitizer **build flags** add `-Wno-error=format-overflow` for that diagnostic, without modifying the game source or regular strict builds |
| Actual F2 Start Game, P1/P2/P3/P4 | PASS separately for counts 1, 2, 3 and 4 on retail ROM. An initial sequential P3 harness run stalled, but its isolated retry passed. |
| Two-player retail combat with shotgun | PASS, distinct first-person views, original AMP visits; P2 fires and spends shells while P1 retains 44 |
| Four-player retail combat / audio | PASS, original-ROM sprites and views, independent movement; per-player weapon sound launches `(3, 3, 3, 7)` in one event-count sample; this is not acoustic output quality certification |
| Four-player Window-resolution 3D | PASS, independent actor motion in 800×600 Xvfb image; viewport data covers separate four frames. Pixel-identity assertion corrected for visible other-player sprites. |
| F12 secondary keyboard rebinds | PASS, sequential P2 `u`, P3 `v`, P4 `w`, each action 0 saved to own profile |
| F12 mouse capture | PASS, P2 mouse button 2 captured via XTEST and persisted |
| Single-player retail rendering | Runs; compared screenshot independently with co-op screenshot; **same black triangular geometry cutout reproduced** (not a multiplayer-only compositor bug). |
| Cross-level travel, full respawn on other maps, mixed species | **NOT IMPLEMENTED IN LIVE GAME**; testable policy/state scaffolding only |
| Physical USB gamepads/multiple mice / Windows | **NOT TESTED** (no user hardware or Windows runtime here) |
| BigPEmu/ROM pixel parity, enemy damage to each player | **NOT VERIFIED** |

## Audit by subsystem and risk

**1. F2 setup / launch:** `pc_video_x11.c` stores setup and starts 1–4 players via the original ROM bootstrap and experimental `local_mp_walktest.c` runtime. F2 launches pass all counts. Individual species selections are still NOT honored by the live same-campaign-species runtime; there is no complete mixed-species initialization, alternate-level start or independent campaign selection. The menu indicates its limitation.

**2. Movement and input:** Original P1 joystick and host analog bridge, separate keyboard/mouse/gamepad profiles and active-low Jaguar pad translation exist. No physical joystick was attached; the live test proves keyboard-driven P2–P4 movement and one-window rendering. S75 blocks P1 residual axes when command capture is rejected. Secondary physical Linux joysticks / mouse paths and Windows Raw Input still need device-in-hand testing. Secondary mouse vertical look and some host quick-turn behavior remain incomplete.

**3. Bindings:** The first P2–P4 Enter-to-bind used a captured key while swallowing Enter's release. Verified stuck-Enter fix and sequential keyboard+mouse captures. P1 keeps original F12 Controls pages. Physical gamepad-button capture not verified. X11 mouse event capture works without a physical USB mouse selected; this does *not* validate multi-device mouse routing.

**4. Weapons/shotgun:** Marine shotgun option grants original slot 1 with 44 shells to P1–P4 in the test. Scope isolation prevents one player's ammo consumption affecting others; F2 retail P2 combat rechecked this revision. Real collected-weapon switching and cross-player attack/hit/damage remain without exhaustive live coverage. No fake weapons added.

**5. HUD and map:** Existing original HUD state capture and per-view renderer map exploration are exercised by native tests; S74 showed independent map hotkeys in retail 2P. **Exact health-bar, score, automap graphic and weapon-overlay parity against Jaguar hardware still cannot be claimed.** The original renderer's dark triangular cutouts occur in **both** 1P and 4P and predate these fixes.

**6. Audio:** S70 added separate weapon sound lanes for up to four players; 4P original-ROM counter test shows events attributed to every player. Effects **mixing, auditory spatialization, volume quality, simultaneous music/ambience and output devices** remain physically unverified; the Jaguar original has no native multiplayer audio rule.

**7. World/enemy AI:** Original AMP update is performed once per shared world with co-op nearest-visible target lock policy. Four-player smoke confirms AMP visits; *actual enemy hit selection, monster-vs-each-player damage and player collision* remain incomplete. The backend and tests support 4 world slots and transaction-safe transfer callbacks, but the **live destination world factory still rejects any level other than the initially loaded one**.

**8. Death/respawn:** Species policy and independent fades are tested. S72 controlled runtime injections exercised same-level Marine P1–P4; those diagnostics are not in the release. This sweep does **not** inject player death. Dying while a new source level is needed stays black instead of traversing worlds, as the loader is deliberately fail-closed. Alien cocoon consumption across levels unverified. Marine resets inventory with configured shotgun, Predator honor resets per policy.

**9. Game feature parity:** Not all single-player F12 systems pass through the experimental `local_mp_walktest.c` loop. Notably **quicksave/quickload (F5/F9), terminals/computers, full pause/Quick Turn and campaign progression** must not be advertised as integrated. Source `pc_main.c` still has a distinct normal single-player gameplay loop. Existing S72 cheat hooks cover a subset of F12 cheats only.

**10. Video:** Actual player AAF sprites appear in other players' views and are depth-sorted with original enemy sprites. Multiple independent HUD frames, real high resolution and smooth-presentation call paths are connected. **Single-player itself shows identical severe black geometry cutouts.** Do not hide this with a multiplayer-specific overlay or claim parity without inspecting original ROM rasterization/port math.

**11. Windows and multi-device:** Linux-only executable runs were performed. No Windows executable was run, no physical controller/multiple-mouse setup was connected, no claims of parity under different PC window managers/GPUs.

## Reproduction (in extracted full source)

1. Configure and build with `cmake -S . -B build -G Ninja -DAVP_BUILD_LOCAL_MULTIPLAYER=ON -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build build --target avp_pc -j8`.
2. Build the native test binaries: `cmake --build build --target test_local_multiplayer_setup test_local_multiplayer_input test_local_mp_gamepads test_local_mp_commands test_local_multiplayer_compositor test_local_multiplayer_worlds test_local_mp_one_window_x11 test_local_mp_retail_maps test_local_mp_retail_rom test_local_mp_collision_original test_local_mp_doors_native test_local_mp_player_native test_local_mp_weapon_native test_local_mp_actor_native test_local_mp_amp_native test_local_mp_native_context test_local_mp_player_step test_local_mp_respawn_policy test_local_mp_life test_local_mp_transition test_local_mp_native_backend test_pc_marine_start_shotgun -j8`.
3. `python3 new/multiplayer/tests/re26_s75_native_regressions.py --build build --rom /path/to/AVP.jag`.
4. `python3 new/multiplayer/tests/re26_s75_bindings_x11.py --exe build/avp_pc --rom /path/to/AVP.jag` (Xvfb and libXtst required).
5. `python3 new/multiplayer/tests/re26_retail_multi_view_smoke.py --exe build/avp_pc --rom /path/to/AVP.jag --players 4 --hires`; and `--players 4 --combat --audio-check` for combat sound attribution.
6. `python3 new/multiplayer/tests/test_f2_start_x11.py --exe build/avp_pc --rom /path/to/AVP.jag --players 4`.

**Release policy:** No retail ROM, emulator internals, fake monster/level loaders, altered gameplay default or tested-only death injection. This source/binary is an **experimental 1–4-player local test**, not a finished 4P campaign.
