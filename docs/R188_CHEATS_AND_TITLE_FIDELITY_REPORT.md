# Alien vs Predator (Atari Jaguar) — RE #19 R188
## ROM-backed title RGB16, new cheats and Facehugger sound preferences

The original Jaguar's game logic and audio cue sites remain authoritative. R188 changes only opt-in PC game rules and uses the existing original death/action/renderer functions rather than inventing replacement actors or a new AI architecture.

### Features and controls

All new cheat entries are in F12 > Cheats. Select **Enemy Type** with Left/Right (or Enter), then choose **Kill Selected Type**. **Kill All Enemies** affects hostile actors currently alive in the loaded level, not other levels or future spawns. Kill methods choose the existing death handling so corpse states and persistence continue through the original pathways. All kill commands avoid scoring synthetic kills because a cheat is not a weapon hit. Queens can still have their original multi-stage story/death logic; this cheat does not bypass campaign scripting.

The **Facehuggers Cannot Attach** switch allows chase/movement and prohibits completing the attach state. **Enemies Cannot Attack** is separate: ordinary hostiles can approach the player but not start attacks or finish damaging hits. Already-launched enemy bolts have special no-player-damage handling on explosion, while the explosive effect still affects nearby AMP objects. Existing world hazards and alien corpse acid remain independent of this cheat.

In F12 > Miscellaneous, **Audio: Facehugged Pain Sounds** and **Audio: Facehug Shake Pain Sounds** default to ON and are saved in `settings.cfg` as `audio.facehug_pain_sounds` and `audio.facehug_shake_pain_sounds` respectively. The general **Audio: Player Pain Sounds** remains unchanged; it is a master mute. Both new controls affect the player's vocal sounds only: normal damage and alternating Left/Right struggle behavior remain.

### Rendering correction retained from R188 title pass

FILES 208 is a 320x280 unscaled Jaguar bitmap displayed in a clipped 320x240 title aperture. R187 resolved the top-line source placement. The currently fully revealed bitmap now passes through the existing RGB16 pack/unpack compositor as its other TOM title objects already do. The exact GPU-resident layout for JPEG FILES 205/207 (title scaled logos) is **not proven** and was not altered.

### Verification limits

Both strict GCC and Clang Release executables compiled; 10 focused checks and an additional direct shake-voice test passed under both. Clang ASan/UBSan passed the AMP/cheat suite. Both builds launched with the verified original 4 MiB AVP.jag to the retail frontend under Xvfb. This is **not** a claim of completed live control testing, hearing the sound, hardware-cycle fidelity, or complete broad-suite testing. See `R188_VERIFICATION.md` for exact status.

Third-party credits and licensing preserved; neither ZIP contains commercial ROM/assets or earlier release archives.
