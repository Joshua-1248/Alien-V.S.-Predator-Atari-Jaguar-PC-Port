# R153 — JERRY wave-ROM discovery and responsive initial dark hold

## Changes

1. **Original waveform search paths.** The separate Jaguar JERRY wave ROM is already supported through `--wave-rom` and beside the supplied cartridge or executable. The Linux release stores executables under `linux_x86_64/gcc/avp_pc` and `linux_x86_64/clang/avp_pc`. R153 also searches **`linux_x86_64/jagwave.rom`** and **`jagwave.rom` at the release root**, in that order after the existing lookups. A custom file passed with `--wave-rom` still takes precedence. The existing SHA-1 check against the canonical 4096-byte JERRY hardware image remains mandatory; R153 does not embed a ROM, alter a patch or invent new wave data. This makes previously implemented authentic synthesis easier to activate with the normal released directory layout, but does not fix the mathematical fallback's approximate waveform shapes.

2. **Cold startup presentation and event handling.** The initial R151 host black interval now displays a cleared Jaguar framebuffer *without* drawing the default title menu prematurely. The X11/Win32 presentation loop polls window/input events during the 60-VBL hold instead of only sleeping. A close request exits cleanly. The existing **one-time** nature of the cold boot hold and its **provisional 60-VBL duration** remain unchanged; returning to title after gameplay never repeats it.

## Fidelity restrictions

- No original 68000 C gameplay files, campaign selection logic, AI, weapons, HUD, save records or score data have changed.
- Original MJP title/menu, three-portrait animation, source-ordered selection music cue, 50-tick zoom and PRESS FIRE gate are unmodified.
- A verified, independently supplied hardware `jagwave.rom` is required for authentic table values; no commercial BIOS/ROM/wave content is included in the release. The AVP cartridge ROM alone does *not* contain JERRY's internal wave ROM.
- No ROM-backed GUI/audio A/B comparison was available in this build environment. Neither the original cold-start timing nor exact character-select synthesizer timbre is claimed complete.

## Wave ROM placement

For a normal release unzip, put a separately obtained and legally supplied **`jagwave.rom`** in the *release root* (the directory alongside `linux_x86_64`), in `linux_x86_64`, beside your chosen executable, or beside your `AVP.jag`. The port recognizes only the exact 4096-byte wave image with canonical SHA-1 `58117e11fd6478c521fbd3fdbe157f39567552f0`. Alternatively launch `avp_pc --rom /path/to/AVP.jag --wave-rom /path/to/jagwave.rom`. The wave ROM is distinct from `AVP.jag`, is not included, and is not required for the port to run with approximate audio.
