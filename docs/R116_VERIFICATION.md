# R116 verification record

- Scope: PC-only window-resolution 3D raster speed-up, **not** Jaguar simulation or original visual parameter changes.
- Full GCC Release build: **PASS**.
- Full Clang Release build: **PASS**.
- Complete GCC C-decomp regression suite: **PASS** (exit 0).
- Complete Clang C-decomp regression suite: **PASS** (exit 0).
- New source-coordinate DDA regression: **PASS** with GCC and Clang. Signed negative/positive projected rows and thousands of successive texture samples match the original integer division.
- Baseline R115 vs R116 synthetic full-frame SHA-equivalent 64-bit FNV checksums: **IDENTICAL** at 320×228, 1280×912, 1920×1080, 2560×1440 and five additional 960×684 camera headings. Source textures and walls are synthesized only for test, not bundled assets. This does not prove every real retail animation frame.
- Representative five-heading 960×684 benchmark on shared test CPU: R115 ~37–66 ms/frame; R116 ~20–31 ms/frame. Larger cases also improved in paired tests, but test-machine load was variable. No claim of universal steady 60 frames per second.
- Xvfb retail ROM startup for both compilers: **PASS**; canonical privately held cartridge recognized, 74 original JagPEG panels decoded, original MJP frontend reached. 9-second test timeout intentionally terminates each running frontend. The test machine has no real ALSA playback device, so live-audio fidelity remains unassessed.
- Original ROM is private, neither linked nor embedded in any release archive.
- Visual/presentation and timing under an actual maximized Linux Mint desktop with GPU compositor: **not measured in this container**; this remains the user's end-to-end verification.
- Original Jaguar derived game code, gameplay timing, collision, enemy AI, sounds and five Predator vision modes: **unchanged from R115**.
- Smart Disc hit audio audibility and original enemy pain-state conditional gating: **still pending**.
