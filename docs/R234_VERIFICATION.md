# R234 verified original-ROM/host-C full sweep (2026-10-27)

**Scope:** read-only, user-owned original retail ROM; independent original-GPU/compiled-host packed RGB16 arithmetic audit; no production modifications. **Observed results** (not assumptions):

| Verification | Actual result |
|---|---|
| Canonical 4 MiB `AVP.jag` source identity | **PASS**, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b` |
| 25 carried-forward original ROM audits | **PASS** |
| New R234 ROM+compiled-host+independent C GPU color test | **PASS**; checksums of whole synthetic 192×144 and 320×144 frames for all 16 runtime stages |
| Total authentic ROM audits | **26/26 PASS** |
| Original GPU instruction interpreter vs new packed-word C reference | **PASS**; 17/17 stages, 1,024 deterministic random pairs each, plus 589,824 full-frame word comparisons |
| Existing compiled PC helper vs retail GPU | **DISCREPANCY OBSERVED**: stages 7, 10–15 differ; others agree in synthetic full-frame tests; details in separate R234 report/log |
| Public source-tree exclusion scan | **PASS** |
| Architecture migration/ownership checker | **1 known pre-existing failure:** `pause OP fourcc PSVO` in PC video backend; identical to R231–R233; no rule weakened and no working pause logic changed |
| Complete GCC C-decomp regression suite | **PASS**, `All C-decomp regression tests passed.`, exit 0 |
| Complete Clang C-decomp regression suite | **PASS**, same final line, exit 0; ran sequentially to avoid `/tmp` collision |
| Strict GCC Release configure + build | **PASS** (`-j4`, native x86_64 ELF mode 0755) |
| Strict Clang Release configure + build | **PASS** (`-j4`, native x86_64 ELF mode 0755) |
| GCC executable vs R233 | **Byte-identical** SHA-256 `7d6051ce9b799e26c03c06646c5508069c7ec60b0c6072b2e49821c63d4e86f6` |
| Clang executable vs R233 | **Byte-identical** SHA-256 `0d8b2b0e4ed445c71ffb636da6fc41098394dce686736c4ae4a999a727d77c3b` |
| `src/`, `include/`, `tests/`, `cmake/`, `CMakeLists.txt` vs R233 | **267/267 byte-identical** |
| GCC retail-ROM headless frontend startup | **PASS to NewTitle/MJP**; externally stopped at 18 s (exit 124, not clean exit) |
| Clang retail-ROM headless frontend startup | **PASS to NewTitle/MJP**; externally stopped at 18 s (exit 124, not clean exit) |
| Release ZIP, executable permission, legal exclusion checks | See packaging test results; never claim unrun gameplay exit or GPU/OP hardware-parity |

**Known fidelity limits:** GPU packed color parity is proven for synthetic data, but the original CPU caller's input-buffer mapping and exact phase-to-OP scanout are not. Real Jaguar bus/Blitter/OP timing, precise DSP Timer1/I2S/audio-waveform parity, video-estimated title/high-score fades, and campaign-wide Jaguar-vs-PC captures are unresolved. No ROM, proprietary game assets, proprietary DSP image or custom cartridge graphics are bundled. The new `tools/retail_cross_rgb16_reference_r234.c` is deliberately *not compiled into the game*. No diagnostics were added to the game.
