# RE26 Sweep 74 — Multiplayer weapon/HUD/map and binding corrections

Baseline: RE26 Sweep 73, original Atari Jaguar AVP retail ROM supplied by user (not distributed).

## Source changes

- Original PC previous/next weapon events consumed during co-op simulation and applied to the player's *bound original MAZESCRN.S weapon state*, not the host's process-global state. Secondary players use their own edge-triggered previous/next actions. Only original available weapons may be selected.
- New-campaign Marine shotgun option applied independently to every actor, including Player 1, with synchronized original PLAYER.S and MAZESCRN.S weapon ownership/pending fields. Normal original first-person weapon script performs equipment/animation; no substitute gun implementation.
- Original `UpdtHUDMsg` / `UpdtHUD` called with the correct actor bound (in combat mode after its weapon script, and during movement-only test after its movement step), refreshing health bar, tracker, score, map toggle, and per-player HUD state.
- Every player's view-scoped retail GPU automap now accumulates its own original map exploration strokes on simulation ticks and reads that same buffer for its HUD, rather than relying on a shared host renderer.
- F12 > Controls > Local Players now supports Enter-to-capture on P2–P4 keyboard, mouse and gamepad action rows. The original P1 binding categories are not replaced. Physical device selection is still needed for multi-mouse/gamepad.
- Expanded original actor/weapon regression tests cover independently scoped original map toggles and health-bar widths, and original next/previous weapon selection without cross-player state mutation.

## Verified this run

- Strict GCC build of full `avp_pc`: PASS.
- Strict Clang `avp_pc`, original weapon and original actor/HUD tests: PASS.
- Multiplayer-disabled GCC `avp_pc`: PASS.
- All 22 regression executables (including X11 single-window and original-ROM resource test): PASS.
- Real original-ROM F2 title menu > Start Game with 4 Marines: PASS, original Marine level 3. All four show original health=1000, shotgun weapon=1, ammo=44 at first and subsequent tick-20/tick-40 logs.
- Original-ROM F2 2-player map-key visual check: PASS. Tab changes only P1 viewport's map region. A separate saved P2 map key changes only P2 viewport's map region. Toggling P1 again leaves P2's map unaffected.
- Automated F12 secondary keyboard capture: one secondary-player assignment saved, but the test navigation landed on P3; full P2/P3/P4 and live mouse/gamepad input capture across physical devices NOT exhaustively validated.
- Manual screenshots/reports before this sweep showed missing P1 shotgun and incorrect health presentation. New live logs and original-HUD machine tests verify *state*, but exact full visual health-bar parity versus Jaguar hardware remains to be checked by user and ROM comparator.

## Still unfinished

- Full mixed-species co-op, destination level loader and cross-level respawns.
- Cross-player damage/hits and object collisions; full audio verification.
- Exact original ROM/HUD and automap presentation parity, dark triangle geometry artifact.
- Physical multi-joystick/multi-mouse and Windows runtime verification.

No ROM, device firmware, licensed emulator internals, or diagnostic lethal injection included. This remains experimental; single-player retained.
