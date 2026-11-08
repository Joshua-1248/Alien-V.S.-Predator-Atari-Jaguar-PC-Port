# Alien vs. Predator (Atari Jaguar) native PC port — R155

## Verified code change

- TOM's 3.5 fixed-point SCBITOBJ sampling is now used for the **character-selection carousel** rather than remapping the decoder source against a rounded destination rectangle. Source index per output pixel `n`: `floor(n * 32 / scale)`; output extent: `ceil(source_count * scale / 32)`.
- The carousel still addresses the original 239 source lines rather than the 240th decoded line. At half scale that yields 120 displayed scanlines, including the partial final line from the authentic vertical REMAINDER behavior. The old R144/R154 119-row test asserted a rounded-rectangle assumption, not the documented Object Processor stepping.
- The selected portrait's zoom starts from the same scaled-object size that the carousel actually displayed; this avoids a 1-pixel discontinuity when selection begins.
- The original `src/game/` C reconstruction, original carousel scale/position tables, and all sound/music pathways are unchanged. No `jagwave.rom` support, no additional ROM requirement, no new assets.
- Added fractional scale framebuffer checks using synthetic colored pixels, not copyrighted artwork. Test covers scale 21/32 and half-scale 320×239 portrait.

## Scope and remaining limits

These changes apply only to CPU-authored SELECT portrait objects. Title-splash image scaling, the later host-driven 50-tick blend animation, global TOM line-buffer timing, JERRY's 40-bit DSP operations, authentic waveform timbre and initial boot hold are **not** declared hardware-exact by this patch. Full 1:1 audiovisual parity still requires testing with the user's `AVP.jag` on a live system. No proprietary wave ROM, bios, cartridge, art or third-party emulator source is bundled.

Hardware source: Atari Jaguar Software Reference Manual / Object Processor, https://asterick.github.io/jag-docs/tom/object-processor.html .
