# Alien vs Predator (1994 Atari Jaguar) PC port — R175

**October 22, 2026 — Game Over and Hall of Fame original-presentation pass**

**Baseline:** R174. **Scope:** hosted front-end display/fade sequence. Original Jaguar `src/game/` routines and score algorithms remain untouched.

## What was compared

The original 4 MiB `AVP.jag` ROM was inspected locally. Its original RDB data holds the SIMULATION TERMINATED multi-line string at ROM-resident text identity `$2AFC3`; it is **not** one generic single-line banner. The original reconstructed `src/game/mjp.c::Do_Lose` invokes two `MJPP` transitions of eight `One_Tick` calls each, with the two 301-tick source waits and the original text. A user-provided BigPEmu video from October 21 shows the High Scores path: title fades black, original engraved background brightens, five ranking rows move upward from below to their final positions, and the green `Press FIRE` prompt appears after they settle. The user-supplied Game Over reference and independent retail screenshot show the source-authored terminated text low on the black screen, around native Y178, with the species graphic above.

## Changes

- `src/jaguar/avp_gpu/frontend_renderer.c`, `include/jaguar/avp_gpu/state.h`: separate, VBlank-driven Game Over entry and exit fades for the two original `MJPP` events; display the original ROM-derived three-line text at the termination layout's lower coordinates using its unchanged 7-pixel glyph advance. Maintain original backdrop and species imagery.
- High Scores mode now begins with a 48-VBlank RGB16 fade of the *original loaded picture*, then moves the already-correct ranking rows upward from offscreen to their original final pixel positions, settling by VBlank 96. The green prompt appears only after the move finishes. The host animation profile is bounded by visual recording; the exact original Jaguar GPU interpolation/OP sprite phrase values are **not proven**.
- `src/platform/pc/pc_runtime.c`: when entering High Scores **from the title**, run a 40-VBlank title fade followed by a brief black hold before showing the Hall of Fame. Do not add another host fade to the Game Over path (it has its original MJP transitions).
- `src/platform/pc/pc_main.c`: when death ending returns while original `MAZE.S` `FadeDown` is still active, finish its existing fade steps by normal `VSync`/`MazeList` calls before switching to frontend. No new death rules, fade speed, or render effects.
- `tests/test_frontend_renderer.c`: verifies the Game Over source-text placement and eight-tick fades and the phased Hall of Fame picture/score/prompt reveal. The modified test fails on unmodified R174 and passes R175. No diagnostics/instrumentation was added to product code.

## Preservation / limits

All 33 original game-source files in `src/game` are byte-identical to R174. Character portraits, R171 corpse persistence, R172 selection fade, R173 loading text gate, R174 SOBJ handoff, audio, user options, score ranking and name-entry logic are preserved. GCC and Clang strict Linux release builds, focused MJP/music/renderer, campaign-end/fade tests and UBSan checks passed. Full long-running gameplay suite and a complete interactive 1:1 pixel/timing reference comparison were **not** performed. Frame timings and termination text offsets in hosted renderer are video-calibrated approximations, not certified hardware-exact Jaguar Object Processor/GPU state. Only original `AVP.jag` is required; neither the game ROM nor other commercial assets are distributed.

# R175 verification — October 22, 2026

| Check | Result | Scope |
| --- | --- | --- |
| Strict GCC Linux Release build | PASS | Full native binary |
| Strict Clang Linux Release build | PASS | Full native binary |
| GCC/Clang `test_frontend_renderer` | PASS | Hall of Fame fade, upward rows, prompt reveal; Game Over text and eight-VBlank transitions; prior renderer checks |
| R175 `test_frontend_renderer` against unchanged R174 renderer | EXPECTED FAIL | First new Hall of Fame intro assertion detects lack of phased state |
| GCC/Clang `test_mjp` and `test_music` | PASS | Original selection/lose CPU paths and sound fade unaffected |
| GCC/Clang `test_campaign_end` and `test_frontend_fade` | PASS | Original ending and RGB16 routines |
| GCC/Clang frontend renderer UBSan | PASS | Undefined behavior checks, `-fno-sanitize-recover=all` |
| Original game files against R174 | IDENTICAL | Every file in `src/game`, 33 total |
| Release ZIP integrity and executable permissions | PASS | After packaging |

**Boundaries:** Original authoring of Jaguar `Do_Lose` and `Do_Fame` is preserved; the hosted presentation algorithm is guided by source `One_Tick` events and user BigPEmu footage, not a complete recovered GPU microcode or a pixel-by-pixel hardware capture. No full gameplay regression, complete interactive Game Over / High Scores comparison, GPU cycle parity or precise audio/video synchronization pass was conducted for this release. The highlighted pass does not prove every effect is 1:1 with hardware. The original `AVP.jag` was used locally for research but is excluded from the release.
