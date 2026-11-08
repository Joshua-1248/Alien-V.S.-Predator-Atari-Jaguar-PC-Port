# R228 Verification — 2026-10-27

## Original retail evidence

- Locally supplied original 4,194,304-byte `AVP.jag`: SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b` — **PASS**.
- 99,072-byte compressed original 68000 program recovered at ROM offset `$3804` — **PASS**.
- New `tools/audit_retail_frontend_timer_callback_chain_r228.py` — **PASS**: both callback installers, zero-count exits, wait helper, both completion branches, saved callback chaining, original frontend callers.
- Preserved `tools/audit_retail_frontend_dispatch_handoff_r227.py` — **PASS**.
- Preserved `tools/audit_retail_frontend_callback_routes_r226.py` — **PASS**.

## Builds and regression tests

| Check | Result |
|---|---|
| Strict GCC Release build, `-j4` | **PASS** |
| Strict Clang Release build, `-j4` | **PASS** (pre-existing linker section-type warning) |
| 10 focused GCC tests | **PASS (10/10)** |
| 10 focused Clang tests | **PASS (10/10)** |
| GCC original ROM frontend under Xvfb | **PASS to NewTitle/MJP**; intentionally stopped at 7-second timeout, exit 124 |
| Clang original ROM frontend under Xvfb | **PASS to NewTitle/MJP**; intentionally stopped at 7-second timeout, exit 124 |
| Production `src`, `include`, `tests` versus R227 | **265/265 files byte-for-byte unchanged** |
| GCC binary vs R227 | **IDENTICAL** `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Clang binary vs R227 | **IDENTICAL** `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| Full complete GCC/Clang regression suites | **NOT RERUN**; R222 is the last documented full dual-compiler pass |

Focused tests: `test_hud_message`, `test_species_weapon_cycles`, `test_title_automatic_menu`, `test_mjp`, `test_alien_real_hits`, `test_frontend_fade`, `test_hud_fade`, `test_hud_weapon_icons`, `test_frontend_renderer`, `test_pc_input_routing`. No diagnostic/instrumentation was added to game code.

## Scope and limits

The new analysis is **retail-ROM static instruction verification** of VBlank timer callback chaining/restoration. The shared `$F1BB40` word is physically in JERRY DSP RAM, not a hardware clock register; its complete DSP-side producer/consumer semantics, original interrupt cycle timing, bus contention and visible Object Processor scanline contents remain unverified. No full hardware A/B image/audio run was performed. Do not claim original CROSS transition rendering parity or any in-game behavior change. Private original ROM, unpacked art/sound, video, local caches and older executable archives are excluded from deliverables.
