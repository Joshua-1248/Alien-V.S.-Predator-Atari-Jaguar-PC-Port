# R224 — Verification record (2026-10-27)

## Original cartridge branch-object audit

- Unmodified private retail `AVP.jag`, 4,194,304 bytes, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`: **PASS**.
- Decompressed original 68000 stream at ROM offset `$003804`, 99,072 bytes: **PASS**.
- `python3 tools/audit_retail_prelist_branch_conditions.py --rom /path/to/AVP.jag`: **PASS**, 44 *distinct* original opcode groups with verified branches. No overwritten opcode windows.
- Exact two original BRANCHOBJ phrase words for both NTSC and PAL video setups: **PASS**.
- Logical branch routing over every even 11-bit VC value for NTSC 240/PAL 280 visible scanlines, with four endpoint assertions each: **PASS**.
- STOP object construction `(0,4)` at `$9308`: **PASS**.
- Cross-reference of condition-code semantics with Atari Jaguar TOM OP technical reference: **PASS**.

## Linux binaries and preservation

| Check | Result |
| --- | --- |
| GCC strict CMake Release with four build jobs | PASS |
| Clang strict CMake Release with four build jobs | PASS |
| GCC focused frontend/gameplay preservation tests | **10/10 PASS** |
| Clang focused frontend/gameplay preservation tests | **10/10 PASS** |
| `src/`, `include/`, `tests/` vs R223 source ZIP | **265 files byte-for-byte identical** |
| Rebuilt GCC ELF hash vs R223 | **IDENTICAL** `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Rebuilt Clang ELF hash vs R223 | **IDENTICAL** `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| GCC ROM-backed frontend startup via Xvfb | PASS, 7-second external timeout (exit 124) |
| Clang ROM-backed frontend startup via Xvfb | PASS, 7-second external timeout (exit 124) |
| Complete regression suites | **Not rerun**; previous R222 full-suite passes remain historical |

The focused suite includes `test_hud_message`, `test_title_automatic_menu`, `test_mjp`, `test_frontend_fade`, `test_hud_weapon_icons`, `test_frontend_renderer`, `test_species_weapon_cycles`, `test_pc_input_routing`, `test_hud_fade`, and `test_alien_real_hits` in each compiler. Both retail-ROM boot logs reached "Retail frontend: translated NewTitle/MJP campaign selection." Test kill signal and missing ALSA devices in Xvfb are not evidence of game crash or audio behavior.

The build tools emitted filesystem clock-skew warnings from source file timestamps, but resulting executables were produced, executed and checked byte-for-byte against the pre-existing R223 binaries. This is not a claim of a clean clock configuration.

## Unverified boundaries

No complete Jaguar-vs-port GPU CROSS image comparison; no hardware-cycle OP list-fetch timings; no input-driven full campaign playthrough, screen-to-screen transition verification or final 1:1 confirmation. No production diagnostic instrumentation or speculative patch. No commercial ROM or proprietary extracted data in the distributable archives.
