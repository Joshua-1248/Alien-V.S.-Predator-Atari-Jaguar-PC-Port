# RE26 Sweep 73 — Original Jaguar player world sprites and retail level catalog

Base: Sweep 72; original retail AVP ROM used locally, not redistributed.

## Live multiplayer player sprites

- Each player's first-person frame now draws **other living players in that same resident native world**, never itself and never an occupant of a different world.
- All visible players are sourced from the existing per-player native actor snapshots. No new player AI, collision actor, synthetic enemy, physics or replacement artwork is introduced.
- Presentational sprite projections use the *original 63 AAF ROM files* and the same `sprite_choice` and `draw_amp_sprite` paths as original AMP characters, including wall depth, window/door occlusion and normal first-person HUD composition.
- `draw_amp_layer` now depth-sorts the original AMP sprites and at most three additional player sprites together. Ordinary single-player renderers submit zero extra players and retain their original behavior.
- Original AAF species sheets are selected using `AC_HUMAN`, `AC_ALIEN`, `AC_PREDATOR`; running/standing is a **new co-op animation selection policy** inferred from actual movement. It is not claimed to be a hidden Jaguar multiplayer animation rule.
- The 60 Hz smooth-presentation setting also interpolates other players' positions and heading. Independent 1–4 first-person views remain in one program and window.
- This is visibility/occlusion, **not yet projectile hits, player-to-player collision or exact multiplayer-specific animation parity**. Existing renderer dark triangular geometry artifacts remain.

## Real retail level catalog integrated into live F2 runtime

- The F2 retail session now builds `AvpMpRetailMaps` directly from all 15 decoded original `FILES.DAT` maze records, with file index/size/dimension validation and duplicate detection, then acquires its active map as a persistent per-(campaign, level) lease.
- The original initial host world is captured into the leased source-verified map. The capture includes already-modified original AMP/door/HUD world state, and the host single-player globals are not overwritten.
- Cleanup returns its map lease and destroys the catalog. Diagnostic no-ROM geometry retains its separate allocation and does not fabricate an original ROM catalog.
- The full map catalog is **not yet an active destination factory**. Safe original AMP/door/HUD initialization, per-level rendering metadata, and transactional joins are still prerequisites to enabling level travel and out-of-level respawns.

## Verification

- Strict GCC and Clang builds: PASS.
- Original multiplayer-disabled build: PASS.
- All 22 existing multiplayer regression executables: PASS. `test_local_mp_retail_rom` uses user-owned Jaguar ROM, 15 levels, original collision tests; one-window test uses Xvfb.
- Actual retail ROM Xvfb four-player combat/movement and independent P2–P4 input smoke: PASS, separate original first-person frames and HUDs; captured screenshots show live original Marine AAF sprites.
- Actual F2 Start Game -> 2-player and 4-player original game tick: PASS.
- This release does not claim full cooperative campaigns, original sprite animation authoring data for PCs, mixed-species play, real four-device audio output, all cheat/UI parity, or completed cross-level transfers.

## Run

    ./avp_pc --rom /path/to/AVP.jag

Enable Local Multiplayer in F12 Miscellaneous, then press F2 at the main menu, configure players, and select Start Game. This remains experimental same-species co-op.
