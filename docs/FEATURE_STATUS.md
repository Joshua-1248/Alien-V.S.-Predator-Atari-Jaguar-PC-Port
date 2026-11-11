# Feature status — RE #36 R116

This native PC port is a work in progress. "Built" or "implemented" does not mean that all scenarios have been verified on a physical Jaguar or through hands-on multiplayer play.

## Available in the source

- Retail-ROM-backed single-player gameplay, original-species campaigns, menus, HUD, and PC audio/video presentation.
- Local multiplayer co-op and deathmatch, multiple local players, bots with configurable behaviors, and Map Maker.
- Player-specific cheat and automap state, local multiplayer settings persistence, and original-rom-backed terminal rendering.
- Procedural JERRY waveforms: **no secondary hardware-ROM dependency**; commercial `AVP.jag` is supplied by each user.
- R116: Balanced Marine bots backpedal while facing a visible Alien during a bounded lure in a narrow choke corridor, across the existing engagement band; safety checks and firing behavior after the lure timeout remain intact.

## Still being refined / not yet proven 1:1

- Full multiplayer bot navigation (door contention, acid-puddle escape and luring in actual retail maps), collision and all map editor pathways.
- Complete Marine ending and attract-story audio/visual comparisons; terminal input/render/audio edge cases.
- Alarm loudness relative to the physical original. Pitch and sound character have been corrected, but the user's next listening test is authoritative for volume.
- Bit-exact Jaguar GPU/Blitter/Object Processor and JERRY hardware output, including timing on physical hardware.

These are open verification and implementation tasks, **not assertions that all other gameplay is perfect**.
