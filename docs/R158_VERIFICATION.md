# R158 Verification — October 21/22, 2026

| Check | Result |
|---|---|
| GCC Release | PASS |
| Clang Release | PASS |
| GCC complete regression suite | PASS: exit 0 and final success marker |
| Clang complete regression suite | PASS: exit 0 and final success marker |
| New independent synthetic arithmetic right-shift oracle, GCC + Clang | PASS |
| Undefined-behavior/signed-overflow sanitizer on right-shift fixture, GCC + Clang | PASS |
| Existing synthetic FullSynth audio regressions | PASS, both compilers |
| Architecture and public asset audits | PASS |
| Original `src/game/` source compared to R157 | BYTE-IDENTICAL (33 files) |
| ZIP CRC integrity and Linux executable permissions | PASS |
| Only runtime game ROM required | `AVP.jag` |
| Sample-by-sample match of JERRY DSP / original character-select music | NOT VERIFIED |

Scope of code change: `src/jaguar/host/fullsynth_host.c`; new `include/jaguar/host/dsp_arithmetic.h`; new synthetic `tests/test_jerry_asr.c`; `tests/run_tests.sh`. Existing Linux GCC/Clang audible output is expected to be unaffected by the explicitly defined sign-extension semantics. No original hardware-ROM data or other copyrighted media were copied, distributed, or requested.
