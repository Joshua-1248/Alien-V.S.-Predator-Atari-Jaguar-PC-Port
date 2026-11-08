# R156 Verification — 2026-10-21/22

| Check | Result |
|---|---|
| Strict GCC Release | PASS |
| Strict Clang Release | PASS |
| GCC complete regression suite | PASS: exit 0 and final success marker |
| Clang complete regression suite | PASS: exit 0 and final success marker |
| Synthetic FullSynth 16-voice positive/negative clipping, GCC and Clang | PASS |
| Architecture and public asset audits | PASS |
| Original `src/game/` code compared to R155 | BYTE-IDENTICAL (33 files) |
| ROM requirement | `AVP.jag` only |
| 40-bit DSP execution and music timbre identical to Jaguar | NOT VERIFIED |

Only the host mixer accumulation width and a synthetic regression changed in the C source. No new ROMs, additional copyrighted asset requirements, instrument reprogramming, soundtrack replacements or diagnostics patches were introduced.
