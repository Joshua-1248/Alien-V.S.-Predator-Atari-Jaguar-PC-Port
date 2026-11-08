# Alien vs Predator (Atari Jaguar) — RE #33 R55

Base: RE #33 R54. The retail Jaguar ROM and all prototype ROMs remain user-owned and are **not** included in this source archive.

## Retail codes, without giving up the F12 menus

The existing **F12 → Cheats → Original / New** structure and inline **Activate For: <Player #>** selector are unchanged. Original cheats remain off by default and session-only. There is now also an active-low Jaguar-pad input decoder at `src/platform/pc/pc_original_cheat_input.c` so the original code sequences can be entered through the configured native Jaguar controls.

* **Hold That Fuji**: hold PAUSE+OPTION; press 1+3, release those digits, then press 2+5+7+9 together. Enables the Fuji shortcuts. The native Jaguar pause/menu and original acknowledgement sound remain untouched.
* **Banana Stops**: press PAUSE to enter the original pause flow; select OPTION, then 6, then 1+3, releasing as needed. Enter **B A 9 A 9 A * OPTION 6 # * * OPTION 2 OPTION** in order. The host interpreter follows `MAZESCRN.S`'s `bnalist`, including both consecutive `*` entries, and no longer requires keeping PAUSE held after opening the pause menu. Enables Banana shortcuts.
* **Global Positioning**: hold PAUSE, press OPTION, release OPTION; press 1+3, release 1+3; press OPTION again. Toggles the already-present per-viewport coordinate overlay (`dec`, `hex`, `l`). The original `show_coords` path inside the game's pause code remains intact.

After Fuji or Banana is active, the following inputs with OPTION held outside Pause use the existing menu-cheat action handlers: 1/2/3/4 toggle weapon ownership, 8 toggles Marine tracker, 6 and 9 adjust clearance, and 1+2+3+4 together refill ammunition. Banana additionally supports OPTION+5 to toggle God Mode, and OPTION+A/B to cycle levels. All commands remain limited by original gameplay state; no gameplay simulation tick or movement timing is changed.

`Cheats → Activate For` changes **menu actions**. Hardware sequence input follows the physical Player 1 Jaguar bindings and targets P1 only, so pressing a code never silently acts on a different person selected in F12. P2–P4 retain the menu interface for Original cheats. Multi-pad keypad input for P2–P4 is not implemented in this release.

## HUD

The Original Banana God Mode menu toggle and Option+5 now both queue the existing `HUD_MSG.S` `CHEAT ON` / `CHEAT OFF` message records through the runtime `set_message` callback, preserving the original five native-tick message lifetime. The multiplayer callback runs within the bound per-player HUD scope. Native source `src/game/hud_message.c` is unchanged. Resetting a campaign or loading a quicksave resets the PC transition latch, preventing an unrelated trailing `CHEAT OFF` message.

Global Positioning's existing X11 viewport overlay appears whenever the menu toggle or original input code enables that mode, showing `dec(xx,yy)`, `hex(aaaa,bbbb)`, and original level numbers 01–15. In split-screen it is scoped to the selected viewport. This uses the established PC overlay, not newly rasterized Jaguar HUD font glyphs.

For original-code compatibility the PC command adapter uses **OPTION+5** to toggle God Mode, following the original command intent, despite the current C transcription's `ExtraKeys()` checking `OPTION+0`. The adapter prevents the same input chord from also firing a non-cheat gameplay command (and masks OPTION+0 while a PC Original mode is active to prevent two conflicting God states).

## Verification

GCC and Clang strict Release multiplayer-enabled builds, original input code tests, and existing F12/menu, R51 bot, awareness, levels, Deathmatch, Map Maker (841), AMP (79), and door flow (42) tests pass. The new input test verifies the exact 15-symbol Banana sequence and that releasing PAUSE early does not accidentally activate Fuji.

Actual play testing with the retail Jaguar ROM and human keyboard/controller input remains necessary to confirm original pause-code ergonomics, visible HUD styling, and audio acknowledgement. Windows/MinGW compilation was not performed.

No `src/game/` files were modified; the source gameplay, physics, AI, HUD message data, and cartridge decoding remain unchanged.
