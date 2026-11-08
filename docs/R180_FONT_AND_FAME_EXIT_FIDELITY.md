# Alien vs Predator (1994 Atari Jaguar) — PC port R180

**Baseline:** R179 (RE #19, October 22, 2026). **Scope:** original retail main-menu/High-Scores glyph-cell fidelity and the High-Scores exit fade.

## Retail ROM font root cause

The exact 4 MiB `AVP.jag` FILES 234 font payload (GZIP-decoded 320x96 CRY16, 61,440 bytes) has glyph source starts **11 pixels apart**, while the R179 portable renderer used `(glyph_index & 31)*10`. The old stride reads shifted/slivered neighboring glyphs, explaining the user's scrambled white main-menu strings and multicolored scoreboard words/numbers. The glyph itself is ten pixels wide; the eleventh is its source spacing. The CPU destination advance was already 11 and is preserved. R180 uses `(glyph_index & 31)*11` in both `fame_font_glyph` and `title_menu_glyph`, with physical source-row bounds checked. Exact same cartridge font, original authored strings, color bands and score digits; no synthesized substitute typeface.

The original ROM brush was privately decoded and examined at source-cell level. A locally rendered source-font sample before/after the change shows that the original labels `SELECT CHARACTER`, `RESTORE GAME`, `HIGH SCORES`, and `CREDITS` become legible with the corrected source stride. Extracted commercial font data/images are **NOT** packaged.

## Recorded exit fade

The original 60-fps BigPEmu reference video `2026-10-21 15-32-16.mp4` contains a real High Scores exit: the picture is still lit at approximately 50.2 seconds, fades to black by approximately 50.9 seconds, then remains black before the title/intro resumes. R179 switched to the title immediately and omitted this fade. R180 now performs a **40-VBlank host-only RGB16 fade of the completed High Scores composition** before loading title resources. It includes the background, rankings, score/name glyphs and green PRESS FIRE, and uses the preexisting `avp_gpu_rgb16_fraction` arithmetic. The fade is distinct from the established title-to-High-Scores entrance. The original score insertion, EEPROM, fire confirmation, frontend CPU logic and title splash remain untouched.

The fade interval follows the recording. It is not a source-proven GPU microcode cycle count; interactive A/B is still necessary for exact 1:1 timing.

## Implementation and tests

Production changes are limited to `src/jaguar/avp_gpu/frontend_renderer.c`, `include/jaguar/avp_gpu/state.h`, and `src/platform/pc/pc_runtime.c`. The existing `tests/test_frontend_renderer.c` font-cell fixture was updated to the authentic 11-pixel stride and now checks the rendered High Scores exit at full, intermediate and black fade phases. The R180 renderer regression passes GCC/Clang; the same revised test fails against the unchanged R179 production renderer on the wrong font-cell origin.

- Strict native Linux GCC/Clang Release builds: **PASS**.
- GCC/Clang focused `test_frontend_renderer`, `test_mjp`, `test_music`, `test_frontend_fade`, `test_campaign_end`, `test_levels_doors`: **PASS**.
- GCC renderer UBSan: **PASS**.
- Clang AddressSanitizer + UBSan combined renderer regression: **PASS**.
- Clang UBSan-only renderer regression (`-O1`) aborts at `free(): invalid pointer` in **both unchanged R179 and R180**. This pre-existing configuration-specific failure is not declared a pass or attributed to R180. A separate combined Clang ASan+UBSan build passes. Further isolated investigation is necessary before certifying the UBSan-only configuration.
- All original `src/game` files: **BYTE-IDENTICAL** to R179.
- Full long-running game regression, real-device hardware trace, and interactive visual A/B: **NOT performed**.

## Distribution

Only current complete source, both Linux executables (mode 0755), current release notes, verification, cumulative R001–R180 addendum, preserved research and attribution/licenses. No commercial ROM, extracted artwork/fonts, wave-ROM, saves, older release ZIPs or private Ghidra files. Supply your own original `AVP.jag` (SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`).

# R180 verification — October 22, 2026

| Check | Result |
| --- | --- |
| GCC strict Release build | PASS |
| Clang strict Release build | PASS |
| GCC/Clang focused frontend renderer | PASS |
| GCC/Clang MJP/musical and campaign-ending checks | PASS |
| GCC/Clang frontend fade and level/door checks | PASS |
| Revised renderer test with unchanged R179 renderer | EXPECTED FAIL (wrong source glyph offset) |
| ROM font 234 source-cell inspection | 11-pixel origin confirmed; locally rendered labels legible |
| High Scores exit black/fade synthetic frame checks | PASS |
| GCC UBSan renderer | PASS |
| Clang combined ASan+UBSan renderer | PASS |
| Clang UBSan-only -O1 renderer | PRE-EXISTING ABORT in R179 and R180 |
| Original 33 src/game files versus R179 | BYTE-IDENTICAL |
| Full long-running gameplay suite | NOT RUN |
| Interactive BigPEmu vs PC live screenshot A/B | NOT RUN |
| Exact Jaguar OP timing/microcode equivalence | NOT PROVEN |

Do not treat synthetic renderer checks as a substitute for an interactive Jaguar-hardware comparison.
