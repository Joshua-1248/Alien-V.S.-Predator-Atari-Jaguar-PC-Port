# RE #10 F2 — MJP GPU fade/cross reconstruction

This note records the provenance and implementation boundary for the MJP GPU
fade work restored after the execution-environment reset.  No retail GPU binary
is redistributed by the public PC-port tree.

## Retail oracle

The retained exact retail GPU payloads were used only as local reverse-
engineering oracles.  They load at Jaguar GPU RAM `$F03000` and are bounded by
retail linker symbols recovered from `RDB.SYM`:

- `cross_label` `$02B828` -> `cross_lx` / `crossx_label` `$02BE00`
- `crossx_label` `$02BE00` -> `crossx_x` / `fadein_label` `$02C528`
- `fadein_label` `$02C528` -> `fadein_x` `$02CA38`

Local oracle payload SHA-256 values used for this reconstruction:

- cross payload (1488 bytes): `0300cb1bf633acbd620f8185b144737fabb596ffce7cb52708d2bf44afb321e5`
- crossx payload (1824 bytes): `d42901b46b285f63ec9363c5f8c627e71b43ea98cd6078b587f278f4b11c8eb0`
- fadein payload (1288 bytes): `3dd5d1d8f74b67bdd09f057c6f70b2247611c4c863c8ac98a60c7022a091c080`

## Pixel arithmetic

The retail programs operate on Jaguar RGB16 pixels.  The channel layout is:

- bits 15..11: Red (5 bits)
- bits 10..6: Blue (5 bits)
- bits 5..0: Green (6 bits)

The 17-way GPU dispatch is not implemented as a floating-point interpolation.
For intermediate phase `k` (1..15), the retail handlers sum independently
truncated binary fractions selected by the bits of `k`:

- 1/2: `(p >> 1) & $7BDF`
- 1/4: `(p >> 2) & $39CF`
- 1/8: `(p >> 3) & $18C7`
- 1/16: `(p >> 4) & $0843`

Phase 0 is zero contribution and phase 16 is a dedicated full-source handler.
`CROSS` performs `A - fraction(A,k) + fraction(B,k)`.  `FADEIN` emits the exact
fraction of its source.

The portable implementation is in:

- `include/jaguar/avp_gpu/frontend_fade.h`
- `src/jaguar/avp_gpu/frontend_fade.c`

The regression suite exhausts every one of the 65,536 RGB16 values across all
17 fade phases.

## SELECT path

Retail selection launches `CROSS`, then later `FADEIN`:

- `SGCF`: cross selected species normal image -> detail image, phases 0..16
- CPU pause remains 50 VBLs; the cross completes and the final image is held
- `SGFI`: fade detail image from dispatch phase 16 down through phase 7
- CPU pause remains 20 VBLs; final 7/16 image is held behind description text

The CPU-owned waits and selection logic remain in `src/game/mjp.c`.  Only the
GPU pixel/state behavior lives in the AVP/Jaguar frontend renderer.

## CROSSX boundary

The exact retail text has one absolute `crossx_label` reference at `$01FB1A`,
inside the `Clear_Bu` region (`Clear_Bu` begins at `$01F9FC`).  Therefore
`CROSSX` is not attached to the SELECT `SGCF`/`SGFI` events.

Its exact 80-line signed seed table and dispatch-phase transform are preserved
as Jaguar-side primitives, but no host event is wired to `CROSSX` until the
remaining `Clear_Bu` parameter/setup path is translated and proven.

## Validation boundary

This checkpoint validates source arithmetic/state behavior by strict build and
regression tests.  Visual comparison against a live verified retail ROM remains
runtime validation and is not claimed by this note.
