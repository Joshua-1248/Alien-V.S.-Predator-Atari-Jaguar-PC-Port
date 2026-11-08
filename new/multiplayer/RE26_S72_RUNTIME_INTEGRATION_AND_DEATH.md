# RE26 Sweep 72 — F2 multiplayer runtime, original cheats, per-player death fades

Base: Sweep 71. Uses the user-owned retail Jaguar AVP ROM from previous sessions.
Never distribute ROM or emulate BigPEmu internals.

## Implemented in the experimental F2 runtime
- Original PC Player 1 simulation joystick-pad and analog axis normalization now enter the 4-player native actor scope. Secondary gamepads preserve independent inputs. **No physical joystick was attached to this automated environment.**
- Original F12 infinite health/ammo, selected restored health/ammo/weapons, tracker, original AMP damage/LOS/no attack toggles, noclip, kill all and kill selected type are now called at the appropriate player/world boundary. **Full F12 feature equivalence is not yet achieved** (notably quicksave/load and live Quick Turn, terminal/campaign handling).
- Restored 60-Hz single-window multiplayer presentation. Smooth Presentation now receives fractional between-tick camera coordinates and GPU render alpha; original AMP/weapon/door snapshot interpolation path is enabled.
- Window-resolution 3D now allocates each player a private framebuffer matching their actual viewport size. 4-player resized-window Xvfb integration verifies 400×300 views inside one 800×600 window. Native HUD overlay is preserved via the renderer's existing upscale logic.
- Removed a duplicated 10-Hz timer increment and repaired the missing presentation call in the incomplete S72 worktree.
- Player death no longer ends the shared session. Each player individually fades from original 60 Hz steps (2 per display frame to fade level 128), resets when the species start level is already loaded, then fades back in. Other players and original single-world AMP/doors continue.
- Original-start actor snapshot resets Marine/Predator inventory, zeroes Predator honor only, preserves other species scores; toggle-dependent Marine shotgun is granted at 44 shells or withheld. Original HUD.S UseCocoon is called within original world/actor scope when a ready cocoon belongs to the current level.
- If death requires a DIFFERENT level from the one already resident, the viewport stays black and the other players continue: the destination-level factory is still unfinished. **Cross-level respawn is NOT implemented**. No fake substitute level loader is supplied.

## Test evidence
- Strict GCC/Clang PC builds PASS; multiplayer-off build PASS.
- GCC native multiplayer 22 regression executables PASS (including new fade and expanded respawn policy tests).
- Clang fade/respawn focused tests PASS.
- ASan+UBSan independent fade test PASS.
- X11 retail four-player original movement and combat/audio test PASS; four sound owner counters `(3,3,3,7)` in the sampled scenario.
- Retail X11 800x600 resized window + four high resolution views PASS, separate input responses PASS.
- Retail F2 Start Game 2- and 4-player keyboard-menu launches PASS.
- **Ephemeral testing-only binary** (NOT included in these archives) injected lethal damage on F2 Marine level 3: P1, P2, P3 and P4 each independently faded and printed a successful respawn at their distinct starting positions. The shipped source has no injection code. The other players remained in the session and their state continued running. This validates the same-world death path, not cross-level travel or all gameplay death sources.

## Outstanding
- Source-derived proper 3D world sprites for other players are not yet connected.
- Original renderer's dark triangular area persists (preexisting known rasterization problem, not eliminated by high-res mode).
- Mixed species, alternate worlds, cross-level respawn and transitions, complete enemy damage and attack behavior, full input device hardware, Windows runtime, terminal/misc original systems, quicksave/load, smooth Quick Turn and complete audio need continued integration.
- **Do not label the current version complete cooperative campaign.**

## Rebuild
```
cmake -S SOURCE -B build -DAVP_BUILD_LOCAL_MULTIPLAYER=ON -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8 --target avp_pc
./build/avp_pc --rom /path/to/AVP.jag
```
In the main menu: F12 → Miscellaneous → Local Multiplayer Enabled, then F2 → Start Game.
