# R229 verification — 27 October 2026

**Baseline:** authenticated R228 complete source and GCC/Clang release. **Scope:** read-only original-Jaguar fade word research and documentation; no changes to the existing production game.

| Verification | Outcome |
|---|---|
| Original `AVP.jag` 4 MiB identity and decompressed 68000 stream | PASS (canonical hashes) |
| New `audit_retail_frontend_dsp_fade_word_r229.py` | PASS (13/13 absolute references, 5 fade callsites, 2 endpoint routines) |
| Previous R228 callback-chain audit | PASS |
| Original fade endpoint callback-count check, step `$FA` | PASS (132 callbacks each direction under stated initial values) |
| GCC CMake Release (`AVP_STRICT=ON`) | PASS |
| Clang CMake Release (`AVP_STRICT=ON`) | PASS |
| GCC focused preservation suite (10 targets) | PASS |
| Clang focused preservation suite (10 targets) | PASS |
| Original-ROM frontend reached via Xvfb, GCC | PASS (18-second external INT timeout, exit 124 is intentional) |
| Original-ROM frontend reached via Xvfb, Clang | PASS (18-second external INT timeout, exit 124 is intentional) |
| GCC binary compared to R228 full release | SHA-256 identical: `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Clang binary compared to R228 full release | SHA-256 identical: `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| `src`, `include`, `tests`, `cmake`, `CMakeLists.txt` versus R228 | 267 files byte-for-byte identical |
| Entire compiler-specific regression suites | NOT RERUN; only focused preservation tests executed |
| Original Jaguar audio waveform and GPU/OP visible-frame parity | NOT VERIFIED |

The original ROM was used locally only and is excluded from both archives; game ROM assets, decoded samples, saves, generated caches, instrumentation patches and prior release ZIP files are likewise excluded. Credits, license and provenance notices are retained.

**Research limitation:** R229 proves the original 68000 fade endpoints and instruction-level CPU writes to DSP local RAM `$F1BB40`; this does not establish a DSP synthesis transfer function, exact timer elapsed time or host-audio mismatch warranting a production change.
