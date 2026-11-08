# R172 — Character-selection fidelity: source/renderer/audio handoffs

## Source crosswalk

- `src/game/mjp.c::Do_Selec()` retains the authored 32-position carousel tables, 11-step expansion, and selection confirmation `SGCF`/`SGFI` event sequence with 50 and 20 Jaguar VBlank pauses.
- The generic `Pause()` routine polls Fire A/B/C and returns immediately when any button remains pressed. In R171, the Fire button that exits the carousel loop remains pressed as the immediate next `Pause()` begins. Both animation intervals may therefore collapse, instantly switching to full-screen art.
- The R172 local `select_animation_pause()` samples the initial held-button mask and only responds to newly pressed A/B/C bits during each transition. It calls the original `One_Tick()` function once per VBlank and preserves the 50- and 20-tick durations unless deliberately interrupted. No extra easing curve or engine-level timing factor is introduced.
- The existing `src/jaguar/avp_gpu/frontend_renderer.c` already implements `frontend_blit_select_zoom_cross`, advancing 0..50 through MJP `TICK` events. It also retains the original 32-step tracks and source resource images. R172 doesn't modify renderer state or artwork.
- Original `End_Sele()` initiates a #$FA FX fade. R171 sent the `FXFA` event to the host front-end, but the PC callback did not call the working reconstructed sound controller. R172 invokes `FXFadeDo(0xFA)` directly; `LEVELS::ScreenOff`'s unchanged `WaitFXFa(); StopMusi();` then prevents gameplay presentation until the music fade is silent. `music.c::fade_tick` does 250-volume-unit steps from 32767 to zero in 132 VBlank iterations.

## Evidence limits

The original Jaguar readable-C baseline can be reviewed in the user's `Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-C-Decompilation`, particularly `src/game/mjp.c` and `src/game/levels.c`. This session verified the current project against that reconstructed C and the user's unchanged 4 MiB retail ROM identity. Direct video timing/frame capture on original Jaguar hardware, exact Object Processor phrase rasterization, and the still-incomplete `SELP` / `SELT` text timing are out of scope for this narrow correction and must not be claimed already matched.

## Regression rationale

`test_mjp` records CROSS, FADEIN, and SELP VBlank event timestamps and proves 50+20 while confirmation remains held; it also exercises an intentional new Fire press after release. R171 fails the new held-confirmation assertion. `test_music` verifies actual FXFadeDo(0xFA) volume/timer semantics; `test_levels_doors` independently verifies the ScreenOff wait/stop ordering; `test_frontend_renderer` checks picture interpolation. No instrumented production code or diagnostics are included.
