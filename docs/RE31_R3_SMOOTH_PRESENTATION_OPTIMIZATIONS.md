# RE #31 R3 — Smooth-presentation micro-optimization (PC multiplayer compositor)

**Baseline:** RE31 R2 complete source. **Scope:** only `new/multiplayer` host presentation (no change to original Jaguar CPU game systems, collision, AI, enemy movement, weapon timing, original GPU rasterizer, inputs, HUD design, or player interpolation).

## Actual hot path

At every host multiplayer display frame, `avp_mp_session_compose()` invokes `avp_local_mp_compose()` to combine up to four separately rasterized original-world/player HUD views. R2 evaluated a 64-bit `(x * (source_width-1)) / (viewport_width-1)` division for *every output pixel*, plus a second pass to clear the entire output surface before overwriting most/all of it. The terminal owner overlay repeated the same per-pixel division. These are PC-only additions, not Jaguar game logic.

R3 maintains **exactly the original nearest-neighbor sample coordinates**, including the last HUD row, rightmost columns, source and destination width=1, odd split screen sizes and nontrivial pitches. It does so by computing a source step + remainder once per viewport, then advancing a source column index with a quotient/remainder carry rather than a division on every pixel. When source and destination widths are identical, it uses `memcpy` per row (vertical sampling is unchanged). The terminal overlay shares this implementation.

R3 also avoids the full-surface clear when every pixel is immediately overwritten (one, two, four, or three with wide-top layout). It clears **only the unused lower-right quadrant** in the classic three-view quadrant arrangement. Output padding is still untouched and invalid input still returns before any destination write.

No heap allocation, SIMD/CPU feature requirements, added frame of latency, or new renderer settings are needed. Presentation remains at the existing 60 Hz frame deadline; this change improves compositor headroom and reduces risk of display deadlines being missed, but **does not itself raise the frame-rate cap**. It does not accelerate single-player drawing, and it cannot guarantee faster whole-game FPS if Jaguar world rasterization, X11 transfer, or another stage is the bottleneck.

## Tests

- GCC Release full build: PASS.
- Clang Release full build: PASS.
- GCC Release with multiplayer disabled: PASS.
- GCC multiplayer executables: 40 PASS / 0 runtime test failures / 3 NOT RUN (retail ROM or display absent).
- Clang multiplayer executables: 40 PASS / 0 runtime test failures / 3 NOT RUN (retail ROM or display absent).
- New `test_local_mp_compositor_exact`: 1,282 complete bitmap comparisons against R2's original 64-bit division formula, including one-pixel source dimensions, arbitrary source/destination pitches, 1–4 players, both orientations, both three-view layouts, nonzero clear colors, terminal overlays, a 1920×1080 composition, and output padding. PASS GCC and Clang.
- Existing `test_local_multiplayer_compositor`: PASS GCC and Clang.
- No retail-ROM gameplay or on-screen 60-Hz frametime capture available in this environment. Windows cross-compilation was not performed.

## Isolated CPU microbenchmark

Reproducible source: `new/multiplayer/tests/bench_local_mp_compositor.c`. Compile this standalone benchmark (Linux/macOS) by linking the compositor plus `local_mp.c`; use the same optimization flag for R2 and R3. Example from source root:

```
cc -O3 -DNDEBUG -Inew/multiplayer \
 new/multiplayer/tests/bench_local_mp_compositor.c \
 new/multiplayer/local_mp_compositor.c new/multiplayer/local_mp.c \
 -o /tmp/avp_bench_compositor
/tmp/avp_bench_compositor 4 0 320 228 1920 1080 0
```

Four 90-frame runs were taken per case on this environment, measuring **only the native CPU composition**, not GPU rasterization or actual game FPS. Representative results (ms/frame; approximate due to host clock/CPU variability):

| Window/viewports | R2 division-based | R3 increment-based | Pixel checksum |
|---|---:|---:|---|
| Four players, 320×228 sources, 1920×1080 output | 5.4–5.9 | 1.3–1.5 | identical |
| Two players, top-bottom, 320×228 sources | 5.5–6.6 | 1.3–1.4 | identical |
| Three players, quadrants, 320×228 sources | 4.3–4.4 | 1.1–1.3 | identical |
| Three players, wide top, 320×228 sources | 5.8–6.4 | 1.4–1.5 | identical |
| Four players, 960×540 sources | 5.5–5.8 | 0.31–0.36 | identical |

**Follow-up to establish perceptible improvement:** capture measured p95/p99 frame intervals in real multiplayer, with the same ROM, scene, player count, resolution, options and host hardware, comparing R2 and R3. If the original rasterizer dominates, further optimization needs a separate source-preserving performance investigation.
