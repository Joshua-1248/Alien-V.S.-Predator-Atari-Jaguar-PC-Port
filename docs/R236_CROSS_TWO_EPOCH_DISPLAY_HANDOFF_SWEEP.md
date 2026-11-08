# R236 — Full sweep: original CPU positioning precedes the CROSS GPU transition

**2026-10-27 · Alien VS Predator (1994 Jaguar) / RE #21. Parent R235.** Authentic 4,194,304-byte retail `AVP.jag` SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. The original ROM remains private and is **not** in either release ZIP. The R236 audit and its output contain no original artwork, soundtrack, CPU/DSP/GPU executable payload, or diagnostic runtime instrumentation.

## High-priority finding: the host's CROSS blend occupies the wrong original interval

Earlier R234 independently authenticated packed 32-bit RGB16 GPU color arithmetic and found mismatches in the PC helper. R235 verified the intro scene's original per-page direction/bounds and the host's fixed-X cross presentation. **R236 connects these directly to the original CPU/GPU *execution order***:

1. **Original CPU interval A — picture positioning without GPU blending.** Retail `$01DF2C` initializes `D5=0`, constructs a 192×144 bitmap object (`Make_New`, `$01DFC8`), increments D5, and calls `One_Tick` (`$01DFD4`). The original `CMP.L #8,D5; BNE $01DF2E` loop continues eight times. Original X positions use the per-scene direction/bounds established in R235.
2. **Original GPU job starts *after* interval A.** The next CPU instructions at `$01DFE4..$01E038` prepare shared GPU command/status and source buffer pointers, set the row count 144, set progress 1, write GPU PC `$F03000` and start it (`$F02114=1`). The CPU also builds an initial picture object before entering the progress loop.
3. **Original concurrent CPU/GPU interval B.** GPU `$F030C4` begins stage 1, publishes progress at `$F030E4`, executes 144 rows per stage and 16 stages, uses the stage-1-only `$7FA4` pointer exchange, and synchronizes with list callbacks at `$7FA8`. CPU `$01E094` reads *GPU* progress and adds **8** before its second, distinct X-position calculation, copies the GPU-published image pointer `$7FA4` to object data `$04BEEC`, calls `Make_New` at `$01E13A`, and repeats until GPU status `$7F80 == 2`. Finally `$01E152` negates direction.
4. **Existing PC approximation.** In unchanged `src/game/mjp.c`, `mjp_transition` posts `MJPP`, executes eight `One_Tick` calls, and posts `MJPS`; it does not reproduce the original post-eight-tick GPU waiting interval. Unchanged `src/jaguar/avp_gpu/frontend_renderer.c` advances a blended scene by **two** color phases per `TICK`, from 0 through 16, **during** those eight host ticks; `MJPS` disables the effect. Therefore the current host places the RGB16 blending interval where the original CPU is still in its eight-object-position loop, before original GPU blending can start.

This is a **verified ordering discrepancy**; it is not merely a speculative difference in transition duration. It does **not** prove exact original number of visible frames: CPU can spin and rebuild objects repeatedly while the GPU is on one stage, list callbacks can run at particular VBlanks, and TOM scanout/bus arbitration affect which completed pixel buffer becomes visible.

## Original intro X coordinates in the two separate CPU loops

These are **CPU-authored Object Processor X positions**, *not photographed display frames*. The first column's eight values come from the original D5 loop; the second column samples what the later CPU loop would calculate for each GPU progress value 1..16. The CPU may read a given stage multiple times or may skip a transient stage depending on synchronization. All source byte checks are in the audit.

| Intro page | Direction | Pre-GPU eight CPU X values | CPU X for GPU progress stages 1..16 |
|---|---:|---|---|
| 0→1 | −1, upper 10 | 58 × 8 | 58 × 6, then 54,50,46,42,38,34,30,26,22,18 |
| 1→2 | +1 | 18,22,26,30,34,38,42,46 | 54,58,62,66,70,74,78,82,86,90,94,98,102,106,110,114 |
| 2→3 | −1 | 114,110,106,102,98,94,90,86 | 78,74,70,66,62,58,54,50,46,42,38,34,30,26,22,18 |
| 3→4 | +1 | 18,22,26,30,34,38,42,46 | 54,58,62,66,70,74,78,82,86,90,94,98,102,106,110,114 |
| 4→5 | −1 | 114,110,106,102,98,94,90,86 | 78,74,70,66,62,58,54,50,46,42,38,34,30,26,22,18 |
| 5→6 | +1 | 18,22,26,30,34,38,42,46 | 54,58,62,66,70,74,78,82,86,90,94,98,102,106,110,114 |
| 6→0 | −1, lower 10 | 114,110,106,102,98,94,90,86 | 78,74,70,66,62,58,58,58,58,58,58,58,58,58,58,58 |

In particular, original second-loop **1→2** begins at X=54 (not 18), whereas the hosted current scene cross always composites at X=18. These data strengthen R235's geometry findings and distinguish the two CPU X regimes.

## What is needed for an exact, non-regressing renderer correction

1. **Reproduce two separate original presentation epochs,** rather than moving the eight currently rendered host blended frames to a different position and claiming parity.
2. **Recover buffer identities at runtime:** original `$7F84..$7FA4`, old/new picture sources, GPU scene destination, phase-1 swap, GPU 144-row writes, and the CPU bitmap pointer used by `Make_New`. Instruction-level initializers and transfer registers are already validated in R217–R221; the remaining missing data is the complete *coupled* live buffer state and publication order.
3. **Determine actual video visibility:** a cycle-aware GPU/Blitter/68000/TOM trace or authentic Jaguar/validated emulator synchronized captures must establish which pixel memory and X are fetched for each active display. Do not turn 16 stages into 16 guessed display frames.
4. **Only then narrow production integration** to original MJP scene transitions. The packed-RGB16 reference from R234 is already verified, but must use original 32-bit pair alignment and source-image ordering. The unrelated selection zoom, title, pause, audio, gameplay, input, HUD, and saves must not change.

## Reproduction

```bash
PYTHONPATH=tools python3 tools/audit_retail_cross_two_epoch_handoff_r236.py \
  --rom /absolute/path/to/your/AVP.jag --source .
```

The audit verifies **49 retail 68000 opcode groups**, **45 GPU instruction checkpoints**, **six original branch targets**, seven intro page scenarios and the actual host MJPP/TICK/MJPS source-event ordering. The previous R235 compiled production scene probe and all other retail research audits are rerun separately. The audit is *not* a dynamic 68000/GPU/Blitter/OP cycle simulator; numeric GPU-progress X values are source-derived candidate CPU values, not a captured display timeline.

**Preservation:** no production file changes, no diagnostics, no arbitrary transition timings or substitute blending, no commercial assets. All original rights/credits and previously working source are retained. The complete regression/build/run results are recorded in `R236_VERIFICATION.md`.
