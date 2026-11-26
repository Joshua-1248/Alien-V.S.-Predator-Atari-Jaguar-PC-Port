# Alien vs Predator Atari Jaguar native PC port — RE28 / S105

## Feature
**Prototype Texture Restoration** is a PC presentation-only F12 > Enhancements toggle, default OFF and persisted as `enhancement.prototype_texture_restoration` in `settings.cfg`.

S105 base: S104 source. Only the optional host renderer, a portable RGB replacement helper, standalone test and offline pack-builder tooling are changed. No Jaguar game logic, enemy AI, multiplayer object ownership, physics or timing is changed. The original retail ROM remains mandatory.

### User-supplied prototype priority
1. October 1993 (7 extracted RGB environment panels)
2. August 18, 1993 (47 extracted RGB panels)
3. April 8, 1994 (63 extracted RGB panels)
4. Retail ROM fallback (56 baseline panels)

For FILES 0..55, 51 slots have conservatively matched historical candidates: **5 October**, **27 August**, **19 April**, and **5 retail** fallbacks (38, 39, 40, 41, 49). These are candidate correspondences based on content similarity, not a guarantee of lossless original textures or enhanced fidelity. Some April panels are nearly identical to retail, and the early source names `JP90` suggest historically compressed JagPEG sources. Never blindly substitute an incomplete/differently shaped alien tile.

The user-generatable pack contains `00.rgb` through `55.rgb` where verified candidates exist. Each file is exactly 49,152 bytes (128×128 RGB888); the packaged source/tool/compiled executable contain no historical commercial texture data. `tools/create_prototype_texture_pack.py` accepts the original user's `.7z` archives via system libarchive or pre-extracted folders, uses Python stdlib only and writes selected raw RGB files with per-file SHA-256 provenance.

When enabled, the PC host uses valid local files from `prototype_texture_restoration/` relative to its working directory (or `AVP_PROTOTYPE_TEXTURE_DIR`) and retains original retail panels for missing/invalid files. The menu explicitly reports how many replacements loaded. Turning OFF restores exact captured retail RGB and CRY buffer contents in place. Texture buffer addresses never change, including when borrowed by up to four multiplayer viewports. Dependent notice composites are updated in place. No change to renderer shading, ray behavior, or native simulation.

## User installation
Extract the S105 Linux build; use `avp_pc_gcc` or `avp_pc_clang` (rename or run directly). Keep original `AVP.jag` as required by the PC port. Run from the game directory:

```
python3 tools/create_prototype_texture_pack.py \
  --oct '/path/to/Oct 1993.7z' \
  --aug '/path/to/1993-08-18.7z' \
  --apr '/path/to/1994-04-08.7z' \
  --output prototype_texture_restoration
```

Then open F12 > Enhancements > Prototype Texture Restoration and enable it. The setting is saved. The pack is separate from the public code/binaries and remains local to the user. No shell/special installation step modifies the original game ROM.

## Verification (2026-11-03)
- Strict Release GCC avp_pc: PASS.
- Strict Release Clang avp_pc: PASS (linker reports existing `.avp_game_state` PROGBITS warning).
- Strict GCC Release `AVP_BUILD_LOCAL_MULTIPLAYER=OFF`: PASS.
- New S105 standalone tests under strict GCC and Clang: PASS. Test covers opt-out default/retail copy, enabling a valid replacement, rejecting a malformed/truncated replacement, preserving RGB/CRY pointer identity, and exact RGB/CRY restoration.
- GCC multiplayer native runnable regressions: **27 PASS**, original retail ROM test **SKIPPED** (ROM unavailable).
- Clang multiplayer native runnable regressions: **27 PASS**, original retail ROM test **SKIPPED** (ROM unavailable).
- X11 compositor test: PASS under xvfb-run for both compilers.
- Offline pack generator: PASS for all three source archives, produced 51 files; decoded raw RGB bytes exactly equal the associated archived PNG RGB bytes for all 51 entries.

## Limitations / remaining verification
- The original 4 MiB retail game ROM was not available for this S105 test. Visual comparison in an actual level and toggle-on/toggle-off co-op rendering still needs retail-ROM playback; no 100% graphical parity or empirical image-quality improvement claim is made.
- Matching from extracted 128×128 PNG pages cannot establish truly uncompressed/original master art. These may be JagPEG-derived images; assess quality in-game and remove replacements that visibly degrade the retail equivalent.
- Sprite, weapon/HUD and original 13 notice overlays are outside S105's replacement scope. The retail decoder remains authoritative when no corresponding candidate exists.
- Data-driven slot match is conservative. Review artwork with original ROM/level tags before treating the selection manifest as final.
