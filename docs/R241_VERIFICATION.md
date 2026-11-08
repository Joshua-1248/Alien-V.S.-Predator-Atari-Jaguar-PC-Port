# R241 verification — CROSS VBlank callback guard and ACK/copy divergence full sweep

**Baseline:** R240. **Date:** 2026-10-27. Authenticated user-owned original retail `AVP.jag`: 4,194,304 bytes, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Neither ROM nor retail assets are redistributed.

| Check | Result |
|---|---|
| Original retail ROM research sweep | **PASS 33/33**: 32 retained original checks + new R241 guard audit. |
| R241 guard/ACK proof | **PASS**: 43 original 68000 opcode groups, three exact BEQ.W target checks, 12 source-derived synthetic cases including four ACK-without-copy outcomes. |
| Distinct 6-/8-/10-record callback guard paths | **PASS**: six- and ten-record empty head branches bypass copy but reach ACK; eight-record copies unconditionally; patch flag changes six-record patch without skipping copy. |
| Original CROSS GPU-launch patch flag | **PASS**: `$4BE20=1` is set before GPU launch. |
| Complete GCC C-decomp regression suite | **PASS** (sequential, exit 0, final full-suite success marker). |
| Complete Clang C-decomp regression suite | **PASS** (sequential, exit 0, final full-suite success marker). |
| Strict GCC and Clang native Linux Release builds | **PASS** both, up to `-j4`. |
| GCC binary compared with R240 | **BYTE-IDENTICAL**, SHA-256 `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6`. |
| Clang binary compared with R240 | **BYTE-IDENTICAL**, SHA-256 `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b`. |
| Production source, headers, tests, build definitions | **267/267 BYTE-IDENTICAL** to R240. |
| Original retail-ROM frontend startup, both builds | **PASS frontend marker** in both; each intentionally terminated at 18 seconds (exit 124), not a clean exit.. |
| Public source-tree resource audit | **PASS**; commercial ROM/media absent. |
| Legacy architecture audit | **PRE-EXISTING FAIL** `pause OP fourcc PSVO`, present in R240 too; no working pause-snapshot code removed, no test weakened. |
| True coupled 68000/GPU/Blitter/TOM timing and visible CROSS frames | **NOT VERIFIED**. Guard cases are synthetic, not a claim of observed Jaguar VBlank scheduling. |
| ZIP source/full integrity, licenses, permissions, hashes | **PASS** after ZIP integrity, content, 0755 mode and SHA-256 verification.. |

**Correction/qualification to earlier documentation:** R239–R240 sometimes described the final `$7FA8=1` as occurring "after the list copy." It occurs after the **copy body or an early guard branch that bypasses it**. `$7FA8=1` does not prove that the source list was nonempty or that any record was written to the staged Object Processor list. This does not establish a real empty-head transition event, tearing, exact scanout timing, frame count or a finished 1:1 CROSS transition.

No production code edits, new diagnostic patches, guessed original timings, or substitute visual/audio behavior. GCC and Clang source/binary release policy, historical addendum, credits and licensing are maintained. See `R241_CROSS_VBLANK_CALLBACK_GUARD_ACK_FULL_SWEEP.md` and `tools/audit_retail_cross_irq_guard_ack_r241.py` for reproducible source evidence.
