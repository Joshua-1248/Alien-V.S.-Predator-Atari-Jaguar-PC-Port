# R234 — original CROSS RGB16 GPU ↔ compiled PC full-frame parity sweep

**Baseline:** R233. **Revision scope:** new original-ROM instruction/compiled-host crossfade parity audit, a separately verifiable packed-word C arithmetic reference, preservation verification and cumulative addendum. No gameplay/renderer/sound/control/save production edits and no diagnostic production patches.

**New verified fidelity gap:** host `avp_gpu_rgb16_cross()` diverges from the original GPU RISC RGB16 packed-word kernels at runtime phases **7 and 10–15**; other runtime phases 1–6, 8–9 and 16 match in exhaustive synthetic 192×144 and 320×144 images. The offline `r234_original_cross_word()` implementation matches original GPU code at all 17 kernel indices. This is a significant, concretely testable parity gap, not yet an authorized blanket live-renderer replacement. Actual original CPU image-buffer mapping and TOM scanout timing remain unresolved.

See `R234_CROSS_RGB16_ROM_VS_HOST_FULL_FRAME_PARITY.md`, `R234_CROSS_RGB16_ROM_VS_HOST_PARITY.log`, and `R234_VERIFICATION.md`. Preserve the working engine and the historical known architecture-guard conflict. Use a separately owned authentic retail ROM; commercial assets are excluded. All licenses, credits, and full cumulative addendum retained.
