# Alien vs Predator (Atari Jaguar) — RE #31 R5

**Baseline:** cumulative RE31 R4 source. No changes to original Jaguar gameplay systems or simulation rates.

## PC Presentation Rate

F12 → Video → PC Presentation Rate now cycles through all **21** requested settings: 24, 30, 50, 60, 75, 85, 90, 100, 120, 144, 165, 180, 200, 240, 280, 300, 360, 480, 500, 540, 1000 Hz. Default is 60 Hz. Settings save to `video.presentation_hz` and reload with validation. The setting is not the physical display refresh selector and will not change the OS/monitor mode.

Host rendering can be scheduled at the selected rate; Jaguar VBL/fades remain 60 Hz and original simulation remains 10 Hz by default. At 240 Hz the frame budget is ~4.17 ms, at 1000 Hz ~1 ms. Actual achievable rates depend on PC rasterization and display throughput.

## PC-only micro-optimization

Multiplayer viewport scaling reuses an already-scaled output row when the next row samples the same original Jaguar source row. No original game code, input, sprites, AI, collision, damage or RNG was touched. Pixel-exact 1,282-case validation passes. In a 2560×1440 four-player isolated CPU compositor benchmark: R4 ~2.30–2.37 ms/frame, R5 ~1.23–1.44 ms/frame (identical checksum). This is **not** a whole-game FPS claim.

## Checks

GCC and Clang strict Release full builds PASS. Multiplayer-disabled GCC PC build PASS. Presentation rate/step tests GCC + Clang PASS; compositor pixel exactness GCC + Clang PASS. Native multiplayer tests **41 PASS / 2 SKIP** for each compiler (retail-ROM skips). Diagnostic startup with 24, 60, 240, 1000 Hz all reached native tick 10. No retail-ROM/1440p sustained-frame-rate measurement or Windows cross-build was performed.

Details: `SOURCE/docs/RE31_R5_HIGH_REFRESH_PRESENTATION.md`.
