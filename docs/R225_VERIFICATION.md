# R225 verification — 2026-10-27

## Retail source authority

- User-owned retail `AVP.jag`, 4,194,304 bytes; SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`: **PASS**.
- Original compressed 68000 code, 99,072 bytes: **PASS**.
- Read-only ROM callback audit `tools/audit_retail_frontend_vblank_callback_lifetimes.py`: **PASS**; 37 original, nonoverlapping instruction windows, four branch targets, all six callback install destinations, `One_Tick` wake loop, and null vs no-op vs list-copy handling.

## Builds, regression and gameplay preservation

| Check | Outcome |
| --- | --- |
| GCC strict CMake Release (`-j4`) | **PASS** |
| Clang strict CMake Release (`-j4`) | **PASS** |
| GCC focused preservation tests | **10/10 PASS** |
| Clang focused preservation tests | **10/10 PASS** |
| GCC original-ROM startup in Xvfb | **PASS** to NewTitle/MJP; process intentionally stopped at 7 seconds (timeout exit 124) |
| Clang original-ROM startup in Xvfb | **PASS** to NewTitle/MJP; process intentionally stopped at 7 seconds (timeout exit 124) |
| R224 `src/`, `include/`, `tests/` comparison | **265/265 byte-for-byte identical** |
| GCC x86-64 ELF vs original R224 release | **IDENTICAL**, SHA-256 `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Clang x86-64 ELF vs original R224 release | **IDENTICAL**, SHA-256 `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| Complete original regression suites | **Not rerun for R225**. R222's historical complete GCC/Clang suites passed. |

Focused tests: `test_hud_message`, `test_title_automatic_menu`, `test_mjp`, `test_frontend_fade`, `test_hud_weapon_icons`, `test_frontend_renderer`, `test_species_weapon_cycles`, `test_pc_input_routing`, `test_hud_fade`, `test_alien_real_hits`. All were rebuilt and executed independently for GCC and Clang. No active game state/asset modifications.

## Important limitations

Runtime game code and PC renderer are identical to R224. This release is an evidence-backed research checkpoint, **not** a claim of completed GPU/OP, pixel-exact CROSS transition or original hardware bus timing. No real controller playthrough, physical Jaguar GPU-cycle trace or synchronized A/B captures. The seven-second original-ROM startup tests were smoke tests only. The expected ALSA errors in headless Xvfb are not gameplay audio verification.

The private retail ROM, GPU code payloads, original extracted assets and videos, game saves, caches, previous releases and production diagnostics are **not** distributed. Attribution/licensing retained; executable modes verified at package creation.
