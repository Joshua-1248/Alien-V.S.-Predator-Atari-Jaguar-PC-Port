# Alien vs Predator (Atari Jaguar) — RE #31 R6

Baseline: cumulative RE31 R5. PC-only performance changes. All original gameplay logic and original Jaguar timing are untouched.

## Micro-optimization 1: multiplayer compositor

`new/multiplayer/local_mp_compositor.c`: prior to drawing each viewport, cache its horizontal source-pixel indices (exact R2 floor(x * (sw-1) / (dw-1)) formula), then gather by index for each distinct source row. Reuses the existing R5 `memcpy` shortcut for subsequent output rows sampling that same source row. No heap allocation, no filtering, no changed source coordinates, colors, pitch handling, terminal overlay, or 3-player clear area. Uses a bounded 4096-element stack array; output widths above 4096 retain the R5 incremental path.

## Micro-optimization 2: PC presentation/window scaler

`include/platform_pc_scale.h`: cache exact floor(x * sw / dw) horizontal sample positions for ordinary widths up to 4096; preserve the original incremental path above that width. Also use a direct `memcpy` for each distinct source row when only vertical scaling is needed. The existing vertical DDA and same-source-row copy remain intact. Applies to host-only single-player presentation and other callers; no original Jaguar CPU, Object Processor, gameplay simulation, sprite timing, renderer math or sound code was changed.

## Verification

- GCC strict Release full build (all targets, including reference): PASS.
- Clang strict Release full build (all targets, including reference): PASS.
- GCC PC build with multiplayer disabled: PASS.
- GCC and Clang available multiplayer tests: 41 PASS, 0 FAIL, 2 SKIP each (original ROM required by the skipped tests).
- Independent-reference compositor pixel equality: 1,291 layouts, all pass, including terminal overlay, padded pitch, 1440p, one-row/one-column dimensions, and >4096 fallback.
- Independent-reference PC scaler equality: 14 fixed + 150 randomized sizes, all pass, including high-resolution world, exact copy, degenerate dimensions, and the 4096/4097 boundary.
- No-ROM/Xvfb diagnostic 2560x1440, presentation_hz=240: reached normal native tick; manually terminated. Not a retail ROM or real monitor frame-pacing validation.
- Windows cross-build, original retail-ROM gameplay, and sustained 1440p/240 Hz not verified.

## Isolated CPU microbenchmarks

Reference machine/compiler: this test environment, GCC -O3. Scripts: `new/multiplayer/tests/bench_local_mp_compositor.c` and `tests/bench_pc_presentation_scale.c` (both compiled -O3 directly against R5 and R6 source). 2560x1440 destination; compositor 4 players, native 320x228 input per viewport.

- Four-player compositor, multiple alternating runs: R5 generally ~1.07–1.41 ms/frame, R6 ~1.01–1.22 ms/frame; source-dependent checksum identical. Variation is expected due to host scheduling and caches. Improvements here do not imply that the full game can meet a 4.17 ms presentation deadline.
- Single-player presentation/window scaler, 320x228 -> 2560x1440: R5 generally ~0.59–0.76 ms/frame, R6 ~0.45–0.51 ms/frame; source-dependent checksum identical.

These are standalone scaling/composition stages, not world rasterization, display transfer, actual FPS or input latency.

The 21 F12 PC presentation rates, including 240 Hz, stay unchanged from R5. 60 Hz remains the default. All previous multiplayer and sprite work is retained.

## Reproduce the isolated benchmarks

From the extracted `SOURCE` directory:

```sh
cc -O3 -std=c11 -Iinclude tests/bench_pc_presentation_scale.c -o /tmp/bench_pc_presentation_scale
/tmp/bench_pc_presentation_scale 320 228 2560 1440
cc -O3 -std=c11 -Iinclude -Inew/multiplayer new/multiplayer/tests/bench_local_mp_compositor.c new/multiplayer/local_mp.c new/multiplayer/local_mp_compositor.c -lm -o /tmp/bench_mp_compositor
/tmp/bench_mp_compositor 4 0 320 228 2560 1440
```

Output checksums must match between R5 and R6 under the same test inputs. Run both in alternating order and ignore first-round warmup for representative comparisons.
