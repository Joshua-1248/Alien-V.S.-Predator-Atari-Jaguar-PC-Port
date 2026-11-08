# R233 complete verification (2026-10-27)

**Revision:** original-ROM research and full project audit only. Original production source and Linux game behavior remain unchanged from R232.

| Scope | Actual observed result |
|---|---|
| Canonical 4 MiB retail AVP.jag authentication | **PASS**, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b` |
| All retail-ROM read-only research checkers | **25 PASS / 0 FAIL** (prior 24 plus new Timer 1/score-descriptor audit) |
| Public source tree exclusions | **PASS** |
| Architecture ownership audit | **1 pre-existing FAIL**: `pause OP fourcc PSVO`, same R231/R232 host-pause pre-copy conflict; neither fixed nor silenced |
| Complete C-decomp regressions (GCC) | **PASS**, `All C-decomp regression tests passed.` (exit 0) |
| Complete C-decomp regressions (Clang) | **PASS**, `All C-decomp regression tests passed.` (exit 0); sequential execution to avoid `/tmp` collisions |
| Native strict GCC Release | **PASS**, fresh CMake build, `-j3` |
| Native strict Clang Release | **PASS**, fresh CMake build, `-j3` |
| GCC game ELF identical to R232 | **PASS**, `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Clang game ELF identical to R232 | **PASS**, `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| Original `src/`, `include/`, `tests/`, `cmake/`, CMakeLists.txt | **267/267 byte-identical** to R232 |
| GCC headless retail-ROM frontend startup | **PASS to NewTitle/MJP**, `Retail frontend: translated NewTitle/MJP campaign selection.` Exit 124 from intentional `timeout -s TERM 18s`; signal 15 on teardown **is not a normal exit**. |
| Clang headless retail-ROM frontend startup | **PASS to NewTitle/MJP**, same logged frontend entry; externally stopped during command termination; normal clean exit **not verified**. |
| Archive ZIP integrity, ELF permissions, legal exclusions | See package validation output; **check before delivery**. |

**Unverified:** real Jaguar cycle-accurate GPU/Blitter/OP scanline picture handoff, Timer 1/I2S interrupt phase and DAC waveform parity, authenticity of default hosted waveforms, exact title-to-select/high-score VBlank effects, original three-campaign A/B hardware gameplay parity. A passing source-level unit suite or timed frontend boot is not proof of completed 1:1 reconstruction.
