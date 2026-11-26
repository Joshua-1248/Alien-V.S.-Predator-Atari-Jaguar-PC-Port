# Game Micro-Optimization (PC-only, opt-in)

The original Jaguar C translation in `src/game/` is deliberately **untouched**.

This folder holds individually validated optional alternatives for selected
original-game routines. The engine always retains the original implementation,
which is used when the enhancement is Off or when an input falls outside the
fast-path proof. `Game Micro-Optimization` is a single F12 Enhancements toggle,
Off by default, stored as `enhancement.game_micro_optimization`.

## First routine: original `SafePos` collision position check

`game_micro_optimization.c` supplies `__wrap_SafePos` to the GNU/Clang PC
linker (`-Wl,--wrap=SafePos`). The original `SafePos` symbol from
`src/game/collision.c` remains linked and is always available through
`__real_SafePos`. The optimized path returns the original `AVP_COLL_SAFE`
result **only** if the original position lies within the map and both 16.16
fractions lie within the original `near`/`far` wall bands; in that case the
retail collision code performs no wall checks and returns Safe. Otherwise the
original collision routine handles the call, including all doors, crossing,
negative coordinates, corner intersections, and world boundaries.

Non-GNU linkers do not install this redirection; the F12 row reports
Unavailable instead of pretending the enhancement is active. This mechanism
is PC-linker-only, not part of the Jaguar original game build.

## How to verify

Build `test_game_micro_optimization` with CMake and run it. It binds the actual
original collision.c to synthetic maze cells and compares On vs. Off for
1,650,000 inputs, including coordinate overflows and varied width values.
Run both GCC and Clang. Retail-ROM playtesting is still required to verify
full-session behavior. The isolated `tests/bench_safe_pos.c` benchmark
measures hot/cold branches; real FPS improvements cannot be inferred from it.

For future routines: add a separately tested fast path here, retain the
original as a fallback, and require deterministic parity + measured speedup.
Do not modify `src/game/` for optimized alternatives.

## RE31 R10: enemy-AI line-of-sight same-cell interior fast path

`__wrap_LineOfSight` now wraps external (including AMP enemy-AI) visibility
checks on supported GNU/Clang PC builds. When the toggle is enabled, **only**
when both endpoints lie within the SAME valid map cell, and each axis' 16-bit
fraction for BOTH endpoints lies in the restricted `0x6000..0x9FFF` interior,
it returns the original visible result (`0`). Every other case calls the
untouched `__real_LineOfSight` from `src/game/collision.c`.

The interior constraint is important: the original fixed-point maze scanner
can report blocked rays even for a very shallow same-cell ray close to a
boundary. The unrestricted same-cell shortcut **failed** parity and was
rejected. This R10 fast path is conservative, non-caching, and preserves door
updates, wall traversal, target movement and original 10 Hz enemy ticks.

`test_game_micro_ai_los` compares optimized On/Off against the real original
code with 1,610,001 cases (randomized wall/door maps, varied coordinates,
exact boundary fractions, and 500,000 targeted interior cases). The independent
`bench_game_micro_ai_los` measures same-cell and far-cell calls separately.
These synthetic tests cannot replace full retail-ROM gameplay testing.

The original `src/game/` files remain byte-for-byte identical to R9. All
routing, toggle state, and alternative routines remain in this folder; only
PC build glue is updated outside it. Unsupported linkers still use original
code exclusively.

## RE31 R11: empty AMP collision-neighborhood fast path

`__wrap_AMPCollisions` optionally bypasses the original 3x3 AMP lookup when
**all nine** `collmap[]` owner IDs around the requested interior cell are
zero. This is a read-only, per-call check of the **live** AMP collision grid;
there is no cache, copy, retimed AI step or altered collision radius.
Any nonzero owner (including stale or invalid values) and every query at or
outside the 64x64 grid boundary is delegated to untouched
`__real_AMPCollisions` in `src/game/collision.c`.

`test_game_micro_amp_collision` compares the original and optimized results
for 2,500,003 positions across empty, populated, stale-ID, invalid and grid
boundary cases. `bench_game_micro_amp_collision` measures two distinct cases:
empty neighborhoods (fast) and registered owners (fallback).
The fast path benefits sparse locations; the extra check incurs a small cost
when an actor is present. The original `/src/game/` source remains unchanged.

GNU/Clang linker entry-point wrapping only affects external callsites of this
original function. The F12 Game Micro-Optimization switch controls this path
alongside R9's SafePos and R10's LineOfSight fast paths; default Off.

## RE31 R13: AMP allocator validation hot path

With the option enabled, the native multiplayer AMP state bridge installs an
opt-in, read-only 300-slot bitmap validator from this folder. It replaces
quadratic repeated comparisons in the host-side AMP state capture/restore
check with a linear walk. The original nested comparisons are kept intact in
`new/multiplayer/local_mp_amp_native.inc` for the disabled/default path. The
callback is installed/uninstalled by the existing F12 toggle, and the
original `src/game/amp.c` is byte-identical to R12. The validator changes no
AMP state, RNG calls, free-list ordering, game tick rate or AI behavior.
Standalone parity and real original AMP state-scope tests validate both.

## RE31 R14: direct AMP world snapshot capture

The native multiplayer world-scope pipeline calls
`avp_mp_amp_native_capture` whenever it binds/restores the original AMP
state for a different local world. The existing staged capture constructs a
75,984-byte local snapshot, validates its free-list and then copies the
snapshot to the destination. After R13's bitmap validation, the extra copy
is a remaining measurable cost.

The PC-side `Game Micro-Optimization` toggle now also enables
`avp_mp_amp_native_set_direct_capture(1)`. The alternate code is located in
`new/game_micro_optimization/amp_capture_direct.inc`, included only by the
optional bridge in `new/multiplayer/local_mp_amp_native.inc`. It validates
all conditions and the currently live free list **before** touching output,
then fills the destination directly. There is no new persistent cache,
thread, heap allocation, changed field layout, or change to the original
`src/game/amp.c`. With the toggle Off, the old staged capture is used
unchanged; with the toggle On, the unchanged original restore is used.

`test_game_micro_amp_scope` compares complete snapshots byte-for-byte
across 1,000 state changes, verifies invalid live-state output nonmutation,
and exercises the real four-world AMP allocator/capture/restore pipeline.
`bench_game_micro_amp_capture` compares staged versus direct capture with
R13's bitmap validator installed in both cases. It does **not** measure
whole-game FPS. The retail ROM / on-monitor 240 Hz comparison remains
outstanding.

## RE49 R403: fixed-point projectile vectors and straight maze traversal

The existing optional switch also wraps `Vector` and `NMoves` (external
calls only). `Vector` replaces the original repeated left-shift loop with
one leading-zero count and the identical unsigned shifts; signs, 32-bit
wrapped coordinate differences, division and output truncation are unchanged.
The INT32_MIN magnitude cases delegate to the existing original-derived
routine. `NMoves` hoists direction and map bounds outside its wall scan,
walks live wall bytes using a fixed stride, and preserves the asymmetric
left/top versus right/bottom edge rule and coarse closed-door classification.
Larger than 64x64 maps use the existing original-derived routine. There is
no cache, AI logic change, floating-point approximation or simulation retiming.

`test_game_micro_vector_moves` compares the actual projected R402 collision
routines with the enhancement off and on, covering 1,500,196 coordinate pairs
(including aliased outputs) and 1,400,001 traversal queries with live map edits.
Its open 64x64 map benchmark uses requested distances of 16 steps; it is not
an overall gameplay or FPS benchmark. The original implementation is always
linked, and all previous micro-optimization paths remain available.

## R404: straight-axis clear line-of-sight

For exactly horizontal/vertical rays with valid endpoints on a Jaguar-size
map, the original scanner never crosses a secondary wall (minor step zero).
The optional alternative bounds the exact number of primary wall tests using
the original 16-bit first distance and strict `< d_max` condition. It walks
live wall bytes using the original cell stride; only zero wall bytes prove a
clear ray. Nonzero wall bytes, the zero-first-distance special case and larger
maps delegate to the original. No cached state or changed wall/door rules.
The same original Micro-Optimization switch selects the new path. The targeted
`test_game_micro_axis_los` compares real original-derived LOS with the switch
on/off for 3,600,001 cases (including reversed rays, live map edits, exact
boundary fractions and invalid endpoints). Benchmarks use open corridors.

## R405: oblique visibility and exact trigonometry

For bounded non-axis rays the optimized LOS path now handles empty wall slots
and unconditional solid walls directly, using the original major-axis and
secondary-crossing fixed-point math. The 32-bit numerator is sufficient on
maps up to 64x64. Zero initial distance, larger maps, and directional walls
or doors needing their original coordinate-specific checks delegate to the
existing scanner. It reads the live map and `fullbits` on each call. Zero
secondary-crossing distance remains the original clear sentinel. This is not
an alternative pathfinder or a change to enemy AI.

`sin_d0` and `cos_d0` now have optional external-call wrappers. The 8 KiB
signed table derives exclusively from the protected original 513-entry sine
table. Reflected quadrants require a separate entry for each 32-angle boundary
and the interior of its bucket. All 65,536 angles match the original routines
exactly for sine and cosine. Original routines remain linked; same-object
internal calls are not necessarily intercepted. The default remains Off.

The R405 LOS test includes open, sparse and mixed/random walls, doors and
fractional boundary cases. Measured isolated results are not whole-game FPS;
complex wall fallback can add overhead. See the release notes and AUDIT logs.

## R406: occupied AMP collision neighborhoods

The existing empty-neighborhood shortcut remains, with a separate row-wise
interior scan for occupied neighborhoods. Original owner validity, live/static
status, exact cell registration, creature widths and strict hit bounds remain
identical; boundary cases and the disabled setting retain the old routine.
No cache or gameplay changes. `test_game_micro_amp_occupied` compares 965,904
additional cases against the original and reports fixed-seed occupancy
benchmarks. See RE49_R406_ADDENDUM.md for results and limitations; dense
occupancy can still incur dispatch overhead compared with the option disabled.

## R407: single-edge SafePos and maze binding dispatch

`safe_pos_edge.inc` handles exactly one wall band when the other axis lies
in the middle. It computes the original wall/door result from the live
wall slot, original flags and known cell coordinates. Corners and
wide overlapping bands retain the original implementation. A PC linker
wrapper in `maze_binding.inc` calls the original `avp_collision_bind_maze`
and mirrors its pointer/dimensions on every bind, even when the option is
Off. No wall, door, or visibility data is cached. SafePos, LOS and NMoves
can read the binding without repeated external getters. The setter has no
internal callers in the original collision translation unit; all four
release executables were checked for a single direct call from the wrapper.
`test_game_micro_safe_pos_edge` verifies 1,792,145 cases, including map
switching while enabled/disabled, wall edits, every relevant door flag
combination and opening byte. See RE49_R407_ADDENDUM.md for measurements.

## R408: nonoverlapping SafePos corners

`safe_pos_corner.inc` implements the optional two-wall corner case with the
same neighboring-wall, reflection, opening and boundary tests as the
original-derived routine. An empty-corner shortcut also checks `fullbits[0]`.
Overlapping wall bands and the disabled setting retain the original path.
`test_game_micro_safe_pos_corner` checks 3,200,001 cases against the original;
its optional command-line benchmark measures four synthetic corner layouts.
See RE49_R408_ADDENDUM.md for measurements and verification limits.

## R409: complete straight-axis visibility fast path

`axis_los.inc` now skips groups of four empty wall slots on supported maps,
recognizes unconditional solid walls directly, and continues from the first
nonempty wall with the exact original wall/door test (height 64 and original
axis reflection). It never caches geometry. The original zero-distance
boundary behavior and unsupported map fallback remain unchanged. The new
path is selected by the existing opt-in toggle. `test_game_micro_axis_los`
now covers 6,090,369 comparisons including all 64 relevant flag combinations,
all 256 door openings, both wall reflections and all four directions at
sampled lateral coordinates. See RE49_R409_ADDENDUM.md for measured results.

## R410 FPS-directed raster sweep

`point_floor_row.h` and `cry_rmw_fast.h` optimize the measured per-frame Software-rendering hot paths, with dispatch controlled by the existing enhancement. See `RE49_R410_ADDENDUM.md` for the actual ROM-backed frame-submission measurements and limitations. `tests/test_frame_hot_paths.c` covers exact CRY and floor-row parity; `tests/framebench_probe_linux.c` is an external Linux x86-64 benchmark probe, not part of any shipped executable.

## R411 high-resolution HUD replay

`hud_replay_fast.h` uses exact tile-local color reuse and disjoint row dispatch for unfiltered non-fixed-hue HUD replay. `tests/test_hud_replay_fast.c` compares against the previous replay at nine resolutions and verifies optional background masks and row partition independence. See `RE49_R411_ADDENDUM.md` for measured full 2560×1440 frame submissions and limitations.

## R412 fused high-resolution occlusion clear

`depth_clear_fast.h` initializes disjoint occlusion rows to the original
FLT_MAX value as part of the existing floor worker job when the enhancement
is on and the textured output is at least 1280×720. Wall workers begin only
after floor workers finish. Smaller/non-textured and disabled paths retain
the prior clear. `tests/test_depth_clear_fast.c` verifies exact values and
partition/guard behavior. See `RE49_R412_ADDENDUM.md` for the measured GCC
mean improvement, effectively flat Clang result and rejected SIMD trial.

R413: direction-specialized point floor/ceiling CRY spans and monotonic out-of-map tail rejection. The original repeated coordinate additions remain exact. See `RE49_R413_ADDENDUM.md` for measured 1440p frame throughput and limitations.

R414: exact power-of-two texel addressing for 128×128 CRY point floor/ceiling spans. See `RE49_R414_ADDENDUM.md` for measured results and limitations.

R415: exact stationary-axis 128×128 CRY floor/ceiling spans; see `RE49_R415_ADDENDUM.md` for the 1440p throughput measurements and limits.

R416: Window-Resolution 3D point-sampled floor pass initializes only uncovered color-buffer pixels instead of clearing the full framebuffer first; see `RE49_R416_ADDENDUM.md` for exact parity, measured 1440p frame throughput, rejected candidate and limitations.

R417: for high-resolution full-window X11 Software gameplay in fullscreen/borderless mode with FPS enabled and V-Sync disabled, `x11_fps_tile.h` caches the exact existing X11 font raster and composites it into the already prepared frame. This avoids the extra full-window back-pixmap transfer while preserving atomic FPS text. `tests/test_x11_fps_tile.c` compares against actual server-rendered reference pixels; `tests/fullscreen_bench_probe_linux.c` is an external diagnostic probe, never linked into the game. See `RE49_R417_ADDENDUM.md` for gating, measured results, and limitations.

R418: synchronous row-partitioned copy of the completed world framebuffer to the X11 image in fullscreen/borderless native-resolution gameplay at 1920x1080 or larger. Reuses the existing raster worker pool; the independent world framebuffer, overlays, and SHM completion protocol are retained. See RE49_R418_ADDENDUM.md for measurements and limitations.

R419: exact opaque 128x128 CRY wall column fast path, GCC only following matched fullscreen measurements. Clang retains R418 behavior; see RE49_R419_ADDENDUM.md.

R420: the existing high-resolution widescreen HUD crop and restore operations use the synchronous raster pool for crops of at least 1920x1080 pixels when Game Micro-Optimization is enabled. Independent source and destination strides preserve the original centered HUD placement and untouched side columns. `tests/test_hud_crop_copy.c` covers both directions and guard cells at six dimensions; see `RE49_R420_ADDENDUM.md` for ROM-backed fullscreen throughput and limitations.
