# Feature status — RE #52 R502

This native PC port is a work in progress. "Built" or "implemented" does not mean that all scenarios have been verified on a physical Jaguar or through hands-on multiplayer play.

## Available in the source

- Retail-ROM-backed single-player gameplay, original-species campaigns, menus, HUD, and PC audio/video presentation.
- Local multiplayer co-op and deathmatch, multiple local players, bots with configurable behaviors, and Map Maker.
- Player-specific cheat and automap state, local multiplayer settings persistence, and original-rom-backed terminal rendering.
- Procedural JERRY waveforms: **no secondary hardware-ROM dependency**; commercial `AVP.jag` is supplied by each user.
- Optional high-resolution replacements, smooth presentation, configurable audio channels and post-processing.
- Software builds and opt-in experimental Vulkan builds completed with GCC and Clang; hardware Vulkan rendering remains unverified.
- R502: Predator bots request decloak on visible enemies and can fire if the cloak state persists; acid-vulnerable bots may traverse unavoidable vent puddles with committed movement and normal acid damage. Controlled native movement tests support these changes; overall bot intelligence remains experimental.
- Marine campaign Facehuggers in Vents modifier adds 3 each in 4B and 4C and 5 each in 5B and 5C, with none added elsewhere.
- Registered software regression suite: 238/238 under GCC and Clang. See [R502 verification and limitations](RELEASE_NOTES_R502.md).

## Still being refined / not yet proven 1:1

- Full multiplayer bot navigation (door contention, acid-puddle escape and luring in actual retail maps), collision and all map editor pathways.
- Complete Marine ending and attract-story audio/visual comparisons; terminal input/render/audio edge cases.
- Alarm loudness relative to the physical original. Pitch and sound character have been corrected, but the user's next listening test is authoritative for volume.
- Bit-exact Jaguar GPU/Blitter/Object Processor and JERRY hardware output, including timing on physical hardware.

These are open verification and implementation tasks, **not assertions that all other gameplay is perfect**.
