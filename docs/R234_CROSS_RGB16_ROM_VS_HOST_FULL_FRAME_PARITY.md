# R234 — original CROSS packed-RGB16 arithmetic vs the actual compiled PC frontend

**2026-10-27.** Baseline R233. **Read-only original ROM**, canonical 4-MiB SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`; the user-owned ROM never enters source archives. No instrumentation or production modifications.

## 1. A newly measured, concrete parity gap

`src/jaguar/avp_gpu/frontend_fade.c` implements `avp_gpu_rgb16_cross(u16 from,u16 to,unsigned phase)`: it computes RGB16 fractions independently for **each 16-bit pixel**, then forms `from - fraction(from,phase) + fraction(to,phase)`.

The original retail CROSS GPU program extracted from RDB.DTA, with original SHA verified, executes **17 distinct GPU RISC kernels** on **packed pairs of 16-bit pixels (32-bit words)**. Runtime dispatches phases **1–16**. These kernels are **not** uniformly equivalent to the current host helper: phase 7 and phases 10–15 change the 32-bit base-word/subtraction sequence to avoid the host's assumed per-pixel interpolation order. A simple 16-bit interpolation coefficient is not sufficient to reconstruct the bit-exact original output.

This audit compiles the *unchanged* existing production C function into a temporary shared library with `cc -Werror`, then directly invokes that binary alongside the original GPU instruction interpreter `gpu_blend_word`. It additionally compiles independent **source-ready, standalone, offline-only** `tools/retail_cross_rgb16_reference_r234.c`, whose operation tables were reconstructed from the actual original RISC LOAD/MOVE/SHRQ/AND/SUB/ADD/STORE kernels. It verifies that reference against the original GPU interpreter **at every phase**. The stand-alone reference is not linked into the shipped game and is not a runtime diagnostic patch.

Comparison uses deterministic generated test pixels, never proprietary Jaguar images, and runs **every 144-row pixel word** of two original-compatible image geometries across all 16 runtime phases:

| Original phase(s) | Synthetic 192×144: mismatching packed-word pairs per affected stage | Synthetic 320×144 |
|---|---:|---:|
| 1–6, 8, 9, 16 | 0 / 13,824 | 0 / 23,040 |
| 7 | 13,816 / 13,824 | 23,027 / 23,040 |
| 10 | 13,819 / 13,824 | 23,027 / 23,040 |
| 11 | 13,816 / 13,824 | 23,027 / 23,040 |
| 12 | **13,824 / 13,824** | **23,040 / 23,040** |
| 13 | 13,816 / 13,824 | 23,027 / 23,040 |
| 14 | 13,819 / 13,824 | 23,027 / 23,040 |
| 15 | 13,816 / 13,824 | 23,027 / 23,040 |

Total comparison count is **221,184 + 368,640 = 589,824 packed 32-bit operations** for the full synthetic scenes; additionally **17 × 1,024** deterministic randomized two-pixel pairs verify all original table entries. The test records per-stage **CRC32 of original-GPU and actual compiled-host outputs**, and every result of the independently reconstructed packed-word C reference must exactly equal the GPU instruction interpreter. See `R234_CROSS_RGB16_ROM_VS_HOST_PARITY.log` for all numerical results.

### Minimal reproduction

```bash
PYTHONPATH=tools python3 tools/audit_retail_cross_host_rgb16_parity_r234.py \
  --rom /absolute/path/to/your/AVP.jag --source . --cc gcc
```

The expected source must be the included release source tree; the ROM is read-only. GCC/Clang strict production builds are separate from this one standalone test compilation.

## 2. Scope / why no speculative live renderer change

This is **proof that the current host crossfade helper is not a general bit-exact implementation of the original CROSS GPU kernels**. It is NOT yet proof that every call in `frontend_renderer.c` uses the same original image-memory inputs, same phase encoding, same output pairing, or same Object Processor scanout timing. In particular `frontend_blit_scene_cross` handles the host's page-cross path, and `frontend_blit_select_zoom_cross` uses that helper in a different coordinate/scale path; the latter's original CPU/OP visual transition requires separate mapping. Replacing the shared helper globally without validating both consumers would risk breaking working effects and violate the project's preservation policy.

The new offline C reference is a verified, non-shipped migration primitive. To integrate safely: (a) trace the original MJP event/caller to GPU phase and its actual source & destination buffer geometry; (b) establish pixel-pair packing and Blitter row boundaries for the specific host effect; (c) reconstruct or capture original stage-to-presented-frame selection and OP scanout; (d) add bit-exact original test pairs/whole-frame comparisons for that effect; (e) only then change the narrowest production call site and rerun full tests. Do not silently replace estimated frontend VBlank durations or unrelated color fades.

## 3. Full-project sweep and retention

- All **26** read-only original retail-ROM research audit tools pass (25 R233 tools, plus this new compiled-host/full-frame CROSS parity tool).
- Default hosted JERRY waveform-ROM path remains non-exact; real DSP interrupt phase, real audio PCM parity and 1:1 campaign playthrough comparison remain open.
- `src/platform/pc/pc_runtime.c` still contains explicit **video-estimated** frontend fade/black holds (40, 188, 10 VBlanks). The original master-audio fade's 132 callbacks are a separate subsystem and must not be substituted for them.
- The existing `PSVO` architecture-guard ownership conflict is unchanged. Do not remove the working high-resolution pause pre-copy or weaken the checker merely to report green.
- Original source files, headers, tests, build definitions and gameplay binaries are unchanged. Refer to `R234_VERIFICATION.md` for actual build, regression and frontend smoke-check results; passing synthetic color arithmetic does NOT certify scanline-exact Jaguar hardware behavior.

Original credits, third-party licenses, historical research notes and the cumulative R1–R234 addendum are retained. Retail ROM/DSP proprietary payload/art/audio are not redistributed.
