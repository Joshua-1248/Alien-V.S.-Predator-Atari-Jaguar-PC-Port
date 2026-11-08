# R152 verification — 21 October 2026

| Check | Result |
| --- | --- |
| Strict GCC and Clang Release builds | PASS |
| Full GCC and Clang `tests/run_tests.sh` | PASS (both final success markers) |
| Focused frontend framebuffer regression | PASS (both compilers) |
| Host renderer controls CROSS phase by original One_Tick | PASS |
| Black border retained outside moving object | PASS |
| Jaguar architecture/public tree audit | PASS |
| All original `src/game/` sources vs R151 | BYTE-IDENTICAL |
| Linux binaries ELF executable permission | PASS (`0755`) |
| ZIP integrity / no previous ZIP included | PASS |
| Side-by-side live Jaguar/PC parity | NOT VERIFIED: no retail ROM here |

This release **does not** claim the select-theme JERRY timbre, the cold-start
60-VBL hold, or the original GPU interpolation are exact. See R152 notes.
