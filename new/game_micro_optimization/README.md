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
