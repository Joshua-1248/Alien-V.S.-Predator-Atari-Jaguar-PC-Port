# Feature status and verification — R71 baseline

This report separates what is **implemented** from what has been **verified**. It should not be used as a warranty or a declaration of 100% parity. Historical R32/R33 notes describe automated tests under GCC and Clang but do not constitute full hands-on retail gameplay certification.

| Area | Present in code | Validation/limitations |
|---|---|---|
| Retail Jaguar single-player: Marine/Alien/Predator | Yes | Original gameplay C path reconstructed; exact visual/audio/CPU hardware parity still under study. |
| Retail game media | Loaded externally | **Retail ROM not bundled**; expected retail-world 4 MiB ROM hash required. |
| Prototype Texture Restoration pack | 51 .prgb panels included | Pack verified against source mappings and CRC/SHA-256; game runtime needs the ROM and the F12 enhancement enabled; public redistribution rights are not established. |
| Native Linux/X11 builds | Yes | Strict GCC and Clang regression releases previously built; fresh clone still needs local build and device testing. |
| Windows host backend | Yes | Earlier cross-build and Wine work documented; current R71 native Windows physical hardware unverified. |
| Offline local co-op, 1–4 people | Yes | Code and regression tests; not guaranteed glitch-free at every scripted transition. |
| Deathmatch / balancing | Yes | Tested in automated cases; 125-HP DM flamethrower balance differs intentionally from retail NPC damage. |
| Up to 12 bots, three species, three behaviors | Yes | R51 and later AI tests; navigation, pickups/awareness, combat geometry still need prolonged live play. |
| Bot diagonal melee geometry | Yes (R69) | Test fails on pre-fix version and passes after fix; live encounters not exhaustively observed. |
| Spawn/respawn and Fast Movement map bounds | Guarded (R70) | Retail level starts and guard tests pass; edge cases may still exist. |
| Map Maker 2D/3D | Substantial support | Scripted door relocation and all playtest/event paths are not complete. |
| Original controller code cheats + F12 toggles | Yes | Input-sequence tests; physical-controller verification and exact HUD font presentation remain work. |
| God Mode / GPS | Yes | Original popup command path and coordinate overlay; overlay uses PC presentation rather than 1:1 Jaguar glyphs. |
| Clear enemies / clear hazards / refresh level | Yes | Actor/door-state tests; preserve campaign and other levels as described; high-impact debug operations. |
| Ambience and random hissing audio separation | Yes | Audio mixer and F12 tests; speaker hardware parity unverified. |
| Extended fog/draw/shooting, smooth presentation | Yes | Optional PC features; performance and cross-device behavior depend on host. |
| Normalize Diagonal Contact Range | Yes (R59) | Opt-in radial harmful-contact check; original behavior remains Off by default. |
| Extend Enemy Activation Distance | Yes (R71) | Original/2×/3×/4×/Unlimited pursuit cutoff tested; no cross-level AI or range-of-attack change. |
| Linux/Windows releases in Git history | No | Repository includes source plus the prototype texture pack, but excludes precompiled binaries. |
| Network/online multiplayer | No | Outside project scope; offline local multiplayer only. |

## Latest notable changes

R59 optional normalized harmful-contact range; R60 projectile collision-grid ownership fix; R61–R64 Marine firing presentation adjustments; R65 enemy clearing; R66 Deathmatch Flamethrower damage 125; R67 hazard clearing; R68 level refresh and Return to Title; R69 F2 Backspace and diagonal bot melee; R70 improved multiplayer spawn/movement guards; R71 extended enemy pursuit activation distance.

See `docs/re33/README_RE33_*.md` and `docs/RE33_R71_RELEASE_NOTES.md` for the technical details. Some historical docs in the source lineage describe **older** states; this table and the latest source take precedence.
