# R189 verification — 23 October 2026

| Check | Result |
|---|---|
| GCC Release configuration and build (strict warnings) | PASS |
| Clang Release configuration and build (strict warnings) | PASS |
| `test_amp` GCC/Clang | PASS |
| `test_enemy_marine` GCC/Clang | PASS |
| `test_enemy_recoil_fidelity` GCC/Clang | PASS |
| `test_amp_projectile` GCC/Clang | PASS |
| `test_amp_routine_closure` GCC/Clang | PASS |
| Clang ASan+UBSan `test_amp` | PASS |
| Live F12 gamepad/keyboard menu interaction | NOT RUN |
| Full gameplay suite and 3-campaign playtest | NOT RUN |
| Actual original ROM SHA-256 | MATCH |
| Both release ELF executable mode bits | VERIFIED |
| ZIP integrity and no embedded commercial ROM/old releases | VERIFIED |

Source delta from R188: `src/platform/pc/pc_video_x11.c`, `src/game/amp.c`, `tests/test_amp.c`, and release documentation. Optional cheat support necessarily changes one reconstructed original gameplay file; with the new cheat inactive, existing behavior is unchanged. No other Jaguar-derived game logic was modified.
