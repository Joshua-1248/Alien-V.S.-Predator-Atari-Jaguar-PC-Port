# R189 release — Facehugger Egg cheat-selection correction

## Corrected

- **F12 → Cheats → Enemy Type** now has eight selections: Alien, Crawling Alien, Marine, Predator, Facehugger, **Facehugger Egg**, Queen and Generator. Facehugger Egg is a distinct target, not an alias for live Facehuggers.
- **Kill Selected Type** on **Facehugger Egg** now destroys living Facehugger Eggs only, using the original `make_dead_egg()` transition to persistent dead egg (`astype=56`, frame 5), clearing the egg's collision-grid owner and preventing it from spawning a Facehugger. The cheat does not award score.
- **Kill All Enemies** now includes living Facehugger Eggs as well as the hostile classes previously covered. It still excludes non-enemy pickups, projectiles and environment props. Dead eggs are not targeted again.
- Enemy Type now wraps modulo eight; no other F12 entries or menu indexes were moved.

## Preserved

- Normal Facehugger chase and attachment rules when cheats are disabled; Facehugger and egg species are separate targets.
- Original egg death behavior, corpse art, persistence, and original scoring on legitimate player weapon damage.
- R188 title RGB16 presentation fix and all existing audio and cheat preferences.
- Copyright/licensing notices, original Jaguar-derived gameplay when cheats are disabled; no original commercial ROM included.

## Verification

Both strict GCC and Clang Release builds compiled. Five focused AMP tests passed under each compiler: `test_amp`, `test_enemy_marine`, `test_enemy_recoil_fidelity`, `test_amp_projectile` and `test_amp_routine_closure`. Clang AddressSanitizer+UBSan `test_amp` passed. New `test_amp` assertions cover independent Facehugger/Egg selection, dead-egg conversion, cleared collision-grid owner, no cheat score, repeat-kill exclusion, inclusion in Kill All, and pickup exclusion. Original retail ROM SHA-256 matches known baseline. Neither broad test suite nor interactive gameplay playtest was run for this targeted correction.
