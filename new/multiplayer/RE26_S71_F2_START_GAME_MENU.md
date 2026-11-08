# RE26 Sweep 71 — Native F2 Start Game menu launch

## Implementation

The title's **F2 Local Multiplayer** menu now has a real selectable **Start Game** row. The PC-only action records a one-shot launch request; the original `Do_Title` loop uses the optional backend callback to return control safely to the PC program. The PC program uses the selected player count and campaign to initialize **the existing retail level** through `pc_begin_campaign`, then launches the already-tested S69/S70 native player/world session. This does not create additional processes, windows or replacement world geometry.

- Menu navigation: Up/Down selects the Start Game row; Enter triggers the request. F2 and Escape back out normally; F12 and the original single-player menu remain available.
- **F2 > Start Game works for 1–4 players** with the original user-provided AVP.jag; use the selected campaign's Marine/Alien/Predator original start. The PC native session is still experimental.
- F12 > Miscellaneous > Local Multiplayer must first be enabled for the F2 entry to appear, as before. `settings.cfg` persists the setup and controls; no new configuration file or command-line argument is necessary.
- **Existing limitation made visible in the F2 UI:** Player 1–4 character choices are not active for this runtime. All players spawn using the selected *campaign* species. The menu states this instead of silently promising mixed-species functionality. Independent level transitions, respawns, enemy combat parity and full co-op campaign completion are still unfinished. Original in-game logic remains unchanged behind the optional host UI seam.
- Both `AVP_BUILD_LOCAL_MULTIPLAYER=ON` and `OFF` compile. The callback field is defined in the shared runtime ABI unconditionally because `avp_pc_core` and the executable have different feature flags. The actual callback code and the original-title extension compile only with `AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE`.

## Reproduction

1. Place your original retail `AVP.jag` beside `avp_pc`, launch `./avp_pc` normally (or let the existing ROM auto-finder locate it).
2. In **F12 > Miscellaneous**, turn on **Local Multiplayer**.
3. At the original title screen, press **F2**. Set **Number of Players** and **Campaign**.
4. Select **Start Game**, press Enter. This enters the actual experimental player session without a command-line switch.
5. Configure the individual controls with **F12 > Controls > Local Players**. The existing game window contains all selected views.

No cartridge data, ROM, Jaguar SDK or proprietary BigPEmu internals are redistributed in the patch or source archive.
