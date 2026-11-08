# R222 — Verification record (2026-10-27)

## Offline retail-ROM evidence

- User-owned original 4,194,304-byte `AVP.jag` SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`: **PASS**.
- Decompressed original 68000 instruction image at cartridge offset `$3804`: length 99,072: **PASS**.
- `python3 tools/audit_retail_cross_op_list_publication.py --rom /path/to/AVP.jag`: **PASS**, 28 CPU instruction checkpoints, fixed OLP write, three original list-copy record layouts, first-phrase pointer mutation using GPU `$7FA4`, and three distinct post-copy `$7FA8` acknowledgments.
- Synthetic model deliberately audits only verified instruction-defined phrase counts/addresses and does not run full 68000, GPU, Blitter, TOM, or bus-level timing.

## Build and regression

| Check | R222 result |
| --- | --- |
| GCC strict CMake Release with four jobs | **PASS** |
| Clang strict CMake Release with four jobs | **PASS** |
| Full GCC `tests/run_tests.sh` suite | **PASS** (`All C-decomp regression tests passed.`) |
| Full Clang `tests/run_tests.sh` suite | **PASS** (`All C-decomp regression tests passed.`, exit 0) |
| 265 `src/`, `include/`, `tests/` file comparison against R221 source archive | **ALL IDENTICAL** |
| GCC executable hash against R221 | **IDENTICAL** (`7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6`) |
| Clang executable hash against R221 | **IDENTICAL** (`0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b`) |
| GCC verified-ROM frontend startup under Xvfb | **PASS** (retail NewTitle/MJP reached) |
| Clang verified-ROM frontend startup under Xvfb | **PASS** (retail NewTitle/MJP reached) |

Both frontend smoke runs used an externally applied six-second SIGTERM timeout (shell code 124). The program reports signal 15 during timeout-induced termination; that is not evidence of an unintended gameplay crash. Xvfb supplies no usable default ALSA device, but sound startup proceeded via host fallback. Complete interactive Jaguar-vs-PC audio/video gameplay A/B was **not** performed.

## Packaging and fidelity constraints

Source, original credits/attribution, original license notices, both current native Linux executable files (mode `0755`), the R222 notes and report, and cumulative addendum are retained. No private ROM, extracted art/audio, EEPROM, cache, generated build objects or earlier release ZIP is redistributed. The host display transition remains R221's: neither its visual fidelity nor original TOM scanline list activation is claimed verified by these tests.
