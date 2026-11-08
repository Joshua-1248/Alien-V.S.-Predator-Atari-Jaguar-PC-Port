# Alien vs Predator (Atari Jaguar) — RE #31 R4

**Baseline:** RE #31 R3 cumulative source. This revision adds an **optional 120 Hz PC-only output cadence**. It does not replace the original Jaguar 60 Hz VBL or the native 10 Hz simulation.

## How to use

F12 → Video → PC Presentation Rate → `60 Hz` or `120 Hz`. 60 Hz remains the default. This value is saved as `video.presentation_hz=60` or `video.presentation_hz=120` in the executable-adjacent `settings.cfg` and is independent of `video.refresh` (the physical monitor mode). The selector exists with and without multiplayer compiled in.

Smooth Presentation should remain enabled for 120 Hz to produce additional interpolated camera/actor frames. Real 120 Hz visibility requires suitable monitor refresh, OS display throughput, and enough raster CPU budget. The original raster may exceed 8.33 ms/frame on large split-screen viewports; in that case the code skips late presentation deadlines rather than replaying missed frames or accelerating game ticks.

## Technical implementation

- `src/platform/pc/pc_video_x11.c`, `include/pc/pc_video.h`: persistent video setting, F12 Video item, getter that returns only 60 or 120. No new device refresh mode is forced.
- `src/platform/pc/pc_main.c`: host world raster uses optional independent output deadline. Existing Jaguar `MazeList()` still advances at 60 Hz and gameplay remains on its unchanged simulation clock. Camera/weapon alpha interpolation now has optional intermediate 120 Hz sampling.
- `new/multiplayer/local_mp_walktest.c`: separate independent `next_render` scheduler. All playable viewports, modal F2, paused screens and deathmatch end presentation use the host renderer; `MazeList`, vent/player fade progression, and world timing remain on the original 60 Hz `next_display` clock. Live F12 rate changes rephase only the render deadline. Terminal exit reinitializes both clocks. Existing 2 ms input polling cap is retained.
- `presentation_interp.{h,c}`: PC-only pure helper to map supported rates to stable integer frame periods. No changes to pose interpolation or original movement.
- `tests/test_presentation_interp.c`: added 60/120/unsupported-rate and independent scheduled VBL vs host clock arithmetic checks.

No original Jaguar CPU, GPU/AMP, AI, physics, weapons, audio RNG, menu simulation, or gameplay frame rates were modified. The prior R3 pixel-identical compositor optimization is preserved.

## Verification performed

- GCC full Release target graph: PASS.
- Clang full Release target graph: PASS.
- GCC Release build with multiplayer disabled: PASS.
- GCC and Clang presentation timing unit tests: PASS.
- Each compiler's runnable multiplayer native tests: 40 PASS. Original retail-ROM tests: 2 NOT RUN (no user-supplied ROM). X11 single-window compositor visual/readback test: 1 PASS per compiler under Xvfb, for 41 executable passes + 2 ROM-dependent unrun cases.
- GCC no-ROM diagnostic startup, 60 Hz and 120 Hz settings, under Xvfb: both entered the main loop and ran 39 native simulation ticks in the 4-second externally terminated smoke test; this does **not** verify retail-ROM rendering or monitor-level 120 Hz delivery.
- No Windows cross-build or retail-gameplay visual check performed.

If 120 Hz stutters or causes CPU pressure at high resolutions, select 60 Hz. Additional rendering does not improve the authored 10 Hz sprite-frame cadence; it improves the number of interpolated host pictures between native updates.
