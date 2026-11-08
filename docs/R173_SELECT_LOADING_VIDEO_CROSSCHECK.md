# R173 — Character selection loading: original-reference video crosscheck

## Reference

User-owned original BigPEmu recording `2026-10-21 15-37-18.mp4`, 1920×1080 at 60 fps, 45.248-second clip. Examined original frames from 24–44 seconds and denser samples from 31–43 seconds. This research note contains no copied visual/audio assets.

- Around 24–26 s, original three-card selection list and original two instruction lines appear.
- Around 28–29 s, selected Colonial Marine picture has expanded; text is beginning.
- By ~31.5–32 s, all seven retail Colonial Marine briefing lines are fully present; green `LOADING SIMULATION...` remains at the lower left. Marine briefing contains 177 glyphs across 7 retail RDB.DTA lines, from address `$2B538`.
- No `PRESS FIRE` visible at 39.0 or 39.25 s; the first observed display is ~39.4 s. Once it appears it remains alongside `LOADING SIMULATION...` until the recorded human confirms and enters gameplay at ~42–43 s; **no evidence of a blink** in the observed interval.
- The 7.4–7.9-second post-text interval is measured from this emulated recording; hence R173's 450 60-Hz VBlank presentation hold is explicitly a calibration, not certified Jaguar machine-code timing.

## R172 mismatch and R173 scope

`pc_front_event` forwarded `SELP` as original cartridge text/status resource pointers, but did not allow the hosted typewriter and loading presentation to finish before `SELS`/`SELT`. The renderer's text already uses the original MJP `TICK` stream for one-glyph-per-VBlank reveal. R173 introduces `frontend_select_loading_ticks`, and waits for the full glyph reveal followed by 450 presentation ticks before forwarding the first original `SELS` sound event and the `SELT` prompt.

This is isolated to the PC host adapter and portable Jaguar frontend renderer. No original `src/game/` translation unit is edited. The same source has reset behavior when entering a new selection screen. Synthetic tests verify 449 ticks are insufficient, 450 ticks are sufficient, and repaint without a `TICK` does not advance the timer. Both compilers compile the real backend, and the unchanged `End_Sele` Fire/music regression suite passes.

## Fidelity not yet proven

The literal 450 VBlank pause models the measured PC-emulator reference, not a precisely decoded Jaguar blitter/loader busy-wait; emulator, console and PAL display timings may differ. Selected portrait rasterization and Jaguar OP/JagPEG pixel identity, actual synchronous cartridge resource load duration, exact text palette/coords, and interactive all-species screenshot A/B remain separate work. Avoid inventing extra fade jobs, loading progress bars, assets or timing dependent on desktop redraws.
