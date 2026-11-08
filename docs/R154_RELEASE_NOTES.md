# R154 — AVP.jag-only JERRY/TOM hardware-source audit

- Removed the previous optional `jagwave.rom` lookup and the `--wave-rom` command-line option, including the external-waveform injection API. **Only `AVP.jag` is needed** as a user-supplied ROM.
- Kept all eight deterministic host-generated JERRY waveform categories. The existing AVP music score, pitch/envelopes, CPU priorities and note scheduling are not modified.
- Refreshed the synthetic audio regression to prove built-in SINE/TRI/NOISE oscillator selection and repeatable initialization without a second ROM.
- Completed an initial source-specific TOM/JERRY hardware audit and prioritized exact source-proven fidelity work. The audit cites the original hardware manuals and other emulator implementations without copying source/firmware/assets.
- All original `src/game/` behavior is preserved; no new ROM, BIOS, wave table dump, commercial art/audio, or extracted media is included.

**Open:** JERRY waveform sample exactness, hardware DSP FM/envelope semantics, per-scanline OP scaling/CRY comparisons, startup VBL timing, and independent original-ROM audio parity. This release **does not** claim to fix the character-selection music timbre.
