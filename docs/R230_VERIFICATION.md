# R230 verification — 27 October 2026

**Parent:** R229 source and Linux x86-64 GCC/Clang release, authenticated retail `AVP.jag`. **Only new research tools and documentation change.** This release does not claim full Jaguar audio fidelity.

| Check | Actual outcome |
|---|---|
| Retail ROM SHA-256 and original embedded gzip CPU SHA-256 | PASS |
| Original `$00B088` DSP header and 2,952-byte image SHA-256 | PASS |
| DSP output opcodes: `SHARQ`, `MOVEI`, `LOAD`, `IMULT`, `SHARQ` | PASS |
| Original DSP gain-word literal placement, stereo register choices, signed-low-16 synthetic arithmetic | PASS, 54 synthetic cases |
| Prior R229 CPU-side 13-reference and 132-step fade audit | PASS |
| Fresh strict GCC Release (`AVP_STRICT=ON`) | PASS |
| Fresh strict Clang Release (`AVP_STRICT=ON`) | PASS (existing `.avp_game_state` section-type linker warning observed) |
| 10 focused preservation tests with GCC | PASS |
| 10 focused preservation tests with Clang | PASS |
| ROM-backed NewTitle/MJP frontend startup, GCC | PASS; externally stopped in headless Xvfb session |
| ROM-backed NewTitle/MJP frontend startup, Clang | PASS; headless display shutdown reported a display I/O error; normal exit not proven |
| Production `src`, `include`, `tests`, `cmake`, `CMakeLists.txt` compared to R229 | 267 files byte-identical |
| Fresh GCC executable vs. original R229 release | SHA-256 identical: `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Fresh Clang executable vs. original R229 release | SHA-256 identical: `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| Complete GCC+Clang regression suites | NOT RERUN; focused tests only |
| Original hardware DSP PCM, original waveform-ROM, exact timing, GPU/OP CROSS transition and full campaigns | NOT VERIFIED |

The ROM is retained locally for these checks, never included in the archives. Original creator credits, licenses and third-party notices are retained. No old release ZIPs, ROM image, extracted DSP binaries, generated game caches, EEPROM/save files, or diagnostic production patches are included.
