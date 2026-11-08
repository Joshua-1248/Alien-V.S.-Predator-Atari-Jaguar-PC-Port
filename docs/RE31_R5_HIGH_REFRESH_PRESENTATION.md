# RE #31 R5 — 21 host presentation rates and PC compositor row reuse

Baseline: RE #31 R4 complete source. Changes are limited to PC presentation-related code.

## F12 Video: PC Presentation Rate

Exactly these 21 sorted values are exposed through the existing left/right and Enter menu controls:

24, 30, 50, 60, 75, 85, 90, 100, 120, 144, 165, 180, 200, 240, 280, 300, 360, 480, 500, 540, 1000 Hz.

Default remains 60 Hz. The option persists as `video.presentation_hz=<rate>` in `settings.cfg` beside the executable. Unsupported values fall back to 60 Hz. The selectable output rate is completely independent of `video.refresh` (the physical screen mode); no unsupported monitor mode is forced. Single-player and multiplayer modes share this option. The authoritative Jaguar VBL, fades, sound/logic, and 10 Hz simulation remain unchanged.

`presentation_interp.h` contains a single host-only rate table, config validation, and menu stepping helper. `presentation_interp.c` maps the validated rate to its integer nanosecond period without modifying original Jaguar gameplay. F12 Video uses the shared helper, including wraparound. The R4 per-render deadline skips missed host slots; it never advances game logic more frequently. 24/30/50 Hz present fewer interpolated frames; higher settings present more only if the machine can render them on schedule.

On a 240 Hz/2560×1440 display, 240 Hz gives approximately **4.17 ms** total host-frame budget. At 1000 Hz, the budget is **1.00 ms**. Neither rate nor any of the >60 options guarantees sustained display updates; the original world rasterizer and X11 transfer remain important costs. Real panel/OS refresh and host presentation cadence need to align for consistent motion. Test 240 Hz on the real monitor with retail gameplay and lower the rate if frame pacing becomes erratic. Extreme frequencies are experimental, intended for benchmarking, not promised effective throughput.

## Micro-optimization: same-source-row reuse in multiplayer compositor

`new/multiplayer/local_mp_compositor.c` now copies a previously completed output row whenever neighboring output rows sample the same immutable Jaguar source row. The horizontal resampling formula and vertical source-row selection are identical to R4; this applies to both the full multiplayer composition and terminal-owner overlay. It allocates nothing, adds no SIMD requirement and never changes player coordinates, gameplay or the source rasterizer. The R3 no-redundant-clear and incremental horizontal sampling optimizations remain intact.

GCC and Clang: 1,282 layout/pixel/terminal/padding test cases matched the original R2 64-bit-division reference. Four-player, 320×228 per-view inputs into a 2560×1440 output, isolated microbenchmark (90 frames × four runs, GCC -O3): R4 ~2.30–2.37 ms/frame versus R5 ~1.23–1.44 ms/frame, same checksum. **These are only CPU compositor timings on the test machine**, not whole-game frame times or a claimed 240 fps.

## Verification

- GCC Release full target graph: pass.
- Clang Release full target graph: pass.
- GCC Release with multiplayer disabled, PC executable and reduced no-reference tool graph: pass. The first full optional-reference-machine build timed out during Musashi compilation; that is not a PC executable failure.
- Presentation timing/menu tests: pass GCC and Clang, all 21 values and forward/backward wraparound.
- Pixel-exact compositor: 1,282 cases pass GCC and Clang; existing layout safety tests pass.
- Multiplayer native suite: 41 passes, 2 retail-ROM-dependent tests skipped, both GCC and Clang; X11 test ran using Xvfb.
- GCC no-ROM/Xvfb diagnostic startup with settings set to 24, 60, 240, 1000 Hz: all survived 3 seconds, reached native game tick 10, and preserved saved settings. This is not a real 1440p/240-Hz frame-delivery measurement.
- Windows cross-compile and retail-ROM in-game visual and pacing checks: not performed.

To profile real frame pacing next: use the same 1440p display mode, ROM/level, all enhancements and same camera path at 60, 120, 144, 240 Hz; measure host presentation cadence and p95/p99 frame time separately from game tick rate. Do not change authored sprite animation speed to hide dropped host frames.
