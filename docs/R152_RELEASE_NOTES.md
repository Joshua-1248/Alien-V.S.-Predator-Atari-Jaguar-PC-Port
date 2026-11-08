# R152 — Original-footage SELECT portrait approach refinement

## Scoped implementation

The user-supplied BigPEmu recording `2026-10-21 15-37-18.mp4` shows the
selected Marine portrait becoming the sole visible card, growing from its
last carousel pose while its background transitions, over approximately
0.8 seconds. Between 27.0 and 27.8 seconds in the recorded sequence the
frame approaches full-screen and background imagery changes. This is
consistent with the 50 original MJP `Pause()` ticks following `SGCF`, but is
*not* proof of the Jaguar GPU's internal interpolation curve.

R151 completed the entire 16-phase renderer effect after only 16 VBLs
(approximately 0.27 seconds), leaving an unnecessary static period before
MJP's 50-tick interval ended. R152 now advances the selected object's size
and position on **each of those 50 existing game ticks**. The Jaguar RGB16
cross arithmetic and 16-step packed-color phase remain in place; the GPU
phase is distributed across the existing MJP interval. Multiple host
redraws within one Jaguar tick cannot progress the motion.

The R151 cross also revealed the destination background across the full
screen before the moving portrait reached those pixels. The reference keeps
uncovered areas black. R152 confines both source and destination image
samples to the expanding portrait's rectangle, retaining black outside it.
The selected object grows to the full-frame image; the subsequent `SGFI`
fade and original PRESS FIRE logic are unchanged.

## Source preservation

Only the hosted Jaguar frontend renderer, its GPU presentation state, and
its framebuffer regression are changed. Every original `src/game/` file is
byte-for-byte identical to R151. No ROM content, artwork, copyrighted sound,
JERRY wave data, or old release ZIPs are added to the package.

## Remaining fidelity gaps

The renderer is now closer to the recorded SELECT motion, but exact OP
fractional scaling, color blending, and inter-frame behavior still need a
live retail-ROM comparison. The startup dark hold is still a provisional
60-frame host interval rather than an exact recovered Jaguar boot timer.
The character-selection score uses the original `$2046C` FullSynth
cartridge descriptor, and it is cued at the first completed three-object
list; its authentic sound depends on correct JERRY synthesis. In the absence
of the optional, externally supplied authentic 4096-byte `jagwave.rom`, the
engine uses mathematical waveform approximations. The app already supports
`--wave-rom` or an authentic `jagwave.rom` alongside the executable. R152 does
not alter music pitch, patch bytes, score events or global gain without
source-backed evidence. No new audio parity claim is made.
