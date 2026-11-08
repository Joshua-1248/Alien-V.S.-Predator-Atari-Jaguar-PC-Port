# R223 — Verification record (2026-10-27)

## Original cartridge audit

- Unmodified user-owned retail 4,194,304-byte `AVP.jag` SHA-256 verified: **PASS**.
- Original 68000 gzip program at physical ROM offset `$003804`, decompressed 99,072 bytes: **PASS**.
- `python3 tools/audit_retail_prelist_vblank_dispatch.py --rom /path/to/AVP.jag`: **PASS**, 39 original instruction groups, 6 branch destinations, VBlank vector `$0100`, ISR callback/fallback dispatch, PRE_LIST two branches and STOP, six frontend callback installation instructions, and three post-copy `$7FA8` acknowledgements.
- Original ROM-only offline audit. No executable ROM content, art, GPU snapshot or private hardware data is redistributed.

## Independent Linux builds and preservation

| Test | Result |
| --- | --- |
| GCC strict CMake Release (-j4) | PASS |
| Clang strict CMake Release (-j4) | PASS |
| Ten focused tests under GCC | PASS |
| Ten focused tests under Clang | PASS |
| `src/`, `include/`, `tests/` comparison against R222 | **265 files byte-for-byte identical** |
| GCC executable SHA-256 vs R222 | **IDENTICAL** `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Clang executable SHA-256 vs R222 | **IDENTICAL** `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| GCC original-ROM frontend smoke under Xvfb | PASS to NewTitle/MJP (externally stopped at 6 seconds) |
| Clang original-ROM frontend smoke under Xvfb | PASS to NewTitle/MJP (externally stopped at 6 seconds) |
| Complete regression suites | **Not rerun in R223**; R222 passed both previously |

The 20 focused tests are `test_hud_message`, `test_title_automatic_menu`, `test_mjp`, `test_frontend_fade`, `test_hud_weapon_icons`, `test_frontend_renderer`, `test_species_weapon_cycles`, `test_pc_input_routing`, `test_hud_fade`, and `test_alien_real_hits`, each run under GCC and Clang. Smoke exit 124 and the signal-15 log are consequences of the external timeout, not a natural exit or game crash. ALSA may log missing audio devices in Xvfb.

## Fidelity limits

No exact physical TOM scanline list-fetch order, GPU/Blitter bus timing, completed original CROSS pixels or matched Jaguar-vs-PC frames are established. Current renderer remains an unverified approximation; R223 intentionally makes no production patch. All normal controls, weapon highlights, music and sounds, HUD, menus, saves, and credits are untouched.
