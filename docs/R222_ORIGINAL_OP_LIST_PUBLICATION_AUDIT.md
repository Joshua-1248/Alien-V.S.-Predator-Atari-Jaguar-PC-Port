# Alien vs Predator (1994 Atari Jaguar) — RE #21 / R222
## Original 68000 Object Processor list staging, fixed OLP and $7FA8 publication audit

**Date:** 27 October 2026. **Baseline:** R221. **Change scope:** original retail-ROM research and portable offline audit script only. No production renderer, gameplay, asset, audio, input, save, ROM resource, or build-definition changes.

### Source-of-truth evidence

Canonical user-owned 4 MiB `AVP.jag`: SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Original gzip-wrapped 68000 program at cartridge offset `$3804` inflates to exactly 99,072 bytes mapped from CPU `$008000`. `tools/audit_retail_cross_op_list_publication.py` verifies **28 original instruction groups** against that program before any derived tests. The ROM is read in place and never bundled, written back, or extracted for redistribution.

**New finding 1 — the active OLP is not the mutable list's source address.** In the original MJP setup, `$01D7BC` loads `$00007548` into D0, `$01D7CA` word-swaps D0, and `$01D7CC` writes `$75480000` to TOM's `OLP` register `$F00020`. The alternate `$01E690` path also writes the same fixed word-swapped value. TOM defines the Object List Pointer as a **word-swapped 32-bit pointer**: the logical starting address is `$00007548`. This value is distinct from both the list-editing area `$04FC00` and the list-copy destination `$050C00`. The separate zero write to `$F00024` is documented only as an observed instruction, *not* mislabeled as OBF (`$F00026`). Jaguar documentation: https://asterick.github.io/jag-docs/tom/object-processor.html .

**New finding 2 — three distinct CPU list-copy paths, all source-identifiable.** The original 68000 code maintains phrase records at `$04FC00` and emits a version at `$050C00`, with `$20` (32-byte) record stepping. Each copied first longword has `+2` applied by the original instruction stream before delivery, rather than being an arbitrary RGB blend effect. These original copy variants were verified:

| Original routine CPU entry | List-copy shape | Special source handling | Wake-up write |
| --- | --- | --- | --- |
| `$01E34A` (list path 6) | 6 records × 4 longwords | Source slot +`$40` first-longword pointer field uses GPU-published `$7FA4` with original `$7FF` mask/left shift. | `$01E3BE`: `$7FA8 ← 1` |
| `$01E4D0` (list path 8) | 8 records × 6 longwords, plus 6-longword tail | No `$7FA4` patch in this routine. | `$01E520`: `$7FA8 ← 1` |
| `$01E804` (list path 10) | 10 records × 6 longwords | Source head record first longword patched using `$7FA4`. | `$01E85E`: `$7FA8 ← 1` |

The offline audit checks original record counts, source/destination address immediates, register-order instruction groups, `+2` operation, `$20` record strides, and all three *post-copy* acknowledgment instructions. Synthetic phrase arrays test the resulting bounded copy/pointer-masking rules without executing CPU/OP hardware or embedding artwork. These copy paths are consistent with the recovered readable C `Lister4`, `Lister3` and `Lister`, but original retail instructions, not the C alone, are the authority.

**New finding 3 — publication is a coordinated operation, not direct screen painting.** The R221 verified GPU instruction loop publishes its image-buffer pointer through `$7FA4` and observes VBlank wake-ups via `$7FA8`. Original CPU `$01E0FA` consumes `$7FA4` into the pending BITOBJ image pointer, and `$01E13A` calls `Make_New`. The new audit proves the three CPU-side list-copy routines post `$7FA8=1` **after** writing their list copies, which is consistent with the GPU's wait. It does **not** prove which path is invoked on each specific scanline or when TOM samples its OLP/list records.

### R221 port comparison and preservation

The port's `src/game/mjp.c` maintains the original source-side object phrase constructors and `Lister4`/`Lister3`/`Lister` source-level copy rules; its `mjp_transition` host seam still advances eight `One_Tick` calls and does not emulate the complete original GPU asynchronous work or TOM's live list scanning. `src/jaguar/avp_gpu/frontend_renderer.c` continues the existing unverified RGB16 page crossblend. **R222 does not replace that effect with a guessed list-swap/screen-fade.** Working weapon HUD, Marine and Alien switching, the analog elevator bridge, Game Over sound, FPS host choices, settings, saves, and all campaigns remain untouched.

### Verification limits and next original-fidelity gate

The ROM opcode match and synthetic record transfer tests establish CPU list-memory operations only. They do **not** establish: the actual `$7548` PRE_LIST records in runtime RAM, indirect branch/caller selection and VBlank interrupt schedule, Object Processor object-list traversal on particular raster lines, RAM access contention with Blitter/GPU, or a matched original Jaguar visible frame sequence. No claim of completed visual parity, full-screen capture parity, or exact 16 displayed frames is made. Next: recover the PRE_LIST construction/link chain from the original cartridge and track the source of the VBlank IRQ callback that selects the list-copy routine, then verify OP phrase sampling against original emulator/hardware traces *before* any visual code change.
