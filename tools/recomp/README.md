# Static recompilation track

This directory begins the replacement of the readable-C game core with an
address-faithful static recompilation path.

M5.0 intentionally starts with `MAIN/ROM.S`, because its historical linked
address and exact 176-byte instruction stream are already independently
reconstructed from `ROM.SYM`/`ROM.DTA`. The generated native function preserves
68000 D/A registers, PC, big-endian Jaguar address space and MMIO calls.

The next expansion target is the decompressed RAM-resident RDB program at
`$00008000`, using the surviving link/symbol maps to generate address-dispatched
native blocks. PC rendering/audio/input remain host backends; original-mode game
state is intended to come from the recompiled machine state.
