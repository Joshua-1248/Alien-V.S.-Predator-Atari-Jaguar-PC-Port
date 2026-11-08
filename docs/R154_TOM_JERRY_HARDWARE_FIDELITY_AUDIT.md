# Alien vs Predator — TOM and JERRY fidelity audit (R154)

**Policy:** AVP.jag is the only required, user-provided ROM. The source/release does not contain the retail game ROM, Jaguar boot/BIOS, the Jaguar internal wave ROM, extracted commercial art/audio, or copied emulator implementation source. Any game-derived cached data remains local and must not be redistributed.

## Hardware reference and architecture

The Jaguar has a 68000 system-manager CPU, TOM (Object Processor, GPU, blitter, video timing and color conversion), and JERRY (DSP, timers, stereo serial interface, and controller interface). These units operate concurrently on Jaguar hardware. This port currently reconstructs the game's CPU decisions and provides purpose-built TOM/JERRY presentation/synthesis functionality; **it is not a cycle-accurate general-purpose Jaguar machine emulator.**

Source references, used as behavior specifications rather than copied code:

- Atari Jaguar Software/Technical Reference cross-index: https://asterick.github.io/jag-docs/architecture/overview.html
- Atari TOM Object Processor: https://asterick.github.io/jag-docs/tom/object-processor.html
- Atari TOM GPU: https://asterick.github.io/jag-docs/tom/gpu.html
- Atari TOM Blitter: https://asterick.github.io/jag-docs/tom/blitter.html
- Atari JERRY DSP: https://asterick.github.io/jag-docs/jerry/dsp.html
- Atari JERRY audio subsystem: https://asterick.github.io/jag-docs/jerry/audio.html
- Documented Jaguar hardware pitfalls: https://asterick.github.io/jag-docs/architecture/hardware-bugs.html
- Virtual Jaguar reference implementation and limitations, **GPLv3 (not copied)**: https://github.com/libretro/virtualjaguar-libretro
- MAME Jaguar core and RISC architecture, **reference only (not copied)**: https://github.com/mamedev/mame/blob/master/src/devices/cpu/jaguar/jaguar.cpp

### TOM: documented requirements and this port

| Hardware behavior | Relevant code | Finding | Next verification |
| --- | --- | --- | --- |
| Object Processor composes scanlines from phrase-aligned bitmap, scaled, branch, GPU and stop objects. | `src/jaguar/avp_gpu/frontend_renderer.c`, `src/jaguar/hw/object_processor.c` | Frontend uses event-driven rendering rather than full OP phrase execution; original visual output is not yet independently proven. | Decode retail MJP's actual object phrases and validate object order, `XPOS`, `YPOS`, and scaling against Jaguar reference captures. |
| `TRANS` omits **logical pixel index/word zero**; it does not discard all dark RGB pixels. | `src/jaguar/avp_gpu/frontend_renderer.c` | The title sprite renderer already keys on packed RGB16 zero, a supported fidelity behavior. | Confirm exact source pixel/color mode for every title portrait object. |
| `HSCALE`, `VSCALE`, and `REMAINDER` are 3.5 fixed-point scaled-object fields; hardware updates vertical remainder on each scanline. | `src/jaguar/avp_gpu/frontend_renderer.c` | Frontend uses image-space scaling approximations. | Reconstruct OP source-pixel stepping/remainder and compare portrait-zoom frame samples, rather than inferring from linear host alpha blending. |
| Read-modify-write (RMW) objects modify CRY components rather than using generic host RGB alpha blending. | `src/jaguar/hw/cry.c` | CRY conversion and RMW logic are Jaguar-owned; still not fully verified at all overlap boundaries. | Compare TOM CRY/RMW and blitter source-shade outputs with manual-defined arithmetic and source-specific fixtures. |
| Blitter registers include width/pitch, clipping, phrase alignment, and arithmetic modes. | `src/jaguar/host/blitter_host.c`, `src/jaguar/avp_gpu/` | Source-derived transfers and screen-specific algorithms exist. They are **not** a general full-register blitter implementation. | Audit only commands actually used in AVP and enforce exact clipping/step/phrase behavior per call site. |
| OP/GPU/blitter share bus/interrupt state with CPU, with video/timer interleaving. | `src/game/`, `src/platform/pc/pc_runtime.c` | Gameplay is reconstructed at the retail 10 Hz control cadence and frontend tick hooks. Individual bus-cycle and scanline timings are not proven hardware-equivalent. | Prefer recovered CPU event ordering and source-assigned VBL counts; compare frame by frame to the supplied BigPEmu captures. |

### JERRY: documented requirements and this port

| Hardware behavior | Relevant code | Finding | Next verification |
| --- | --- | --- | --- |
| DSP executes 16-bit opcodes with GPU-like semantics plus 40-bit MAC, circular addressing and signed saturation. | `src/jaguar/host/fullsynth_host.c` | FullSynth is a purpose-built behavioral reconstruction, **not** instruction-accurate DSP emulation. Prior project notes report five unresolved indirect branches in the recovered DSP program. | Resolve those source-program branches and DSP address/phase semantics before modifying modulation or envelopes. |
| Eight 128-entry 16-bit signed waveforms occupy **2,048 bytes of unique samples** at `$F1D000–$F1DFFF`; accessible as 1,024 sign-extended 32-bit words. | `host_wave_table[8][128]` | Current built-in mathematical tables match the eight **categories**, not the original waveform contents. | Establish source-backed sample formulas/phase lookup from hardware manuals or non-copyrighted derivations; explicitly label approximation until verified. |
| FullSynth descriptors include distinct waveform selection for carrier and modulator. | `host_wave_sample()`, `synth_sample_fmenv()` | Both waveform pointers are separately resolved; unsupported DSP addresses return silence, not an arbitrary sine. | Compare period/index fraction, signed shift and envelope row arithmetic against recovered DSP data flow. |
| PIT and stereo DAC/serial registers determine sequencer tick and output cadence. | `avp_jaguar_fullsynth_mix()`, `avp_jaguar_fullsynth_start_music()` | The synth uses a 20,774 Hz source path and a rational NTSC PIT accumulator. Sample-exact timing against Jaguar not yet established. | Validate divisor, prescaler, PCM sample clock, stereo channel processing and pitch drift using ROM-derived events plus supplied recorded audio. |
| CPU software selects scores/effects and manages voice priorities/loop release. | `src/game/music.c`, `src/jaguar/host/fullsynth_host.c` | R153 already uses original score/patch addresses from the verified retail `AVP.jag` and preserves original CPU score-control path. | Do not change those descriptors or the working voice arbitration speculatively. |

### Policy implemented in R154

1. Remove `--wave-rom` and all automatic searches for `jagwave.rom`.
2. Remove the optional 4 KiB external hardware-ROM injection interface and associated buffers. No hardware image is created, embedded, cached, or required.
3. Preserve deterministic, mathematical eight-wave in-memory synthesis on `AVP.jag` alone. Audio output with no external wave ROM is intended to be unchanged by this cleanup.
4. Replace the optional-ROM test with an all-synthetic test of deterministic oscillator output on repeated initialization.
5. Leave the original 68000 gameplay sources, music score control, display ordering, and host caches intact.

### Prioritized next engineering work (not claimed completed)

1. **JERRY DSP source exactness:** audit the recovered FullSynth DSP image's five indirect branches; derive oscillator lookup, 40-bit MAC/saturation, attack/release and stereo mixing directly from that code. Only then alter synthesis with a source-backed regression.
2. **ROM-only audio A/B:** if the user supplies an `AVP.jag` accessible in this session, render the character-selection score from its original descriptor and compare to the provided BigPEmu video audio. This is the only sound comparison that can substantiate a timbre/tempo correction.
3. **TOM Object Processor:** implement exactly the `HSCALE`/`VSCALE`/`REMAINDER` and `TRANS` behavior required by the cartridge's object phrases, not an unrelated generic GPU. Compare title/carousel frames to BigPEmu.
4. **Blitter and CRY:** compare selected AVP 68000 commands with hardware mode/control registers and original CRY arithmetic. Preserve source-proven behavior already working in the project.
5. **Startup clock:** replace the provisional 60-VBL host-only black hold only when exact Jaguar source or reliable capture timing establishes the original duration.

### Verification limits

- The provided build environment contains the PC port sources and reference videos, but not a separately mounted verified retail `AVP.jag` for direct audiovisual parity checks.
- Mathematical waveform shapes are **not** sample-identical to undocumented Jaguar silicon contents; this cleanup does **not** constitute a character-select music timbre fix.
- Hardware references/other emulators were studied as documentation. No emulator code, bundled ROM or extracted proprietary tables were introduced into this release.
