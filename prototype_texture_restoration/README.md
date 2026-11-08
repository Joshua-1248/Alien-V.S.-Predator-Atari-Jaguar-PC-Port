# Optional Prototype Texture Restoration pack

This directory contains **51 AVPTEX1 `.prgb` texture panels** with CRC32-checked 128×128 RGB data. They are used by the F12 **Prototype Texture Restoration** enhancement. The port requires a separate, compatible retail Jaguar ROM.

Use from the repository root:

```bash
python3 tools/verify_prototype_texture_pack.py prototype_texture_restoration
```

The executable searches this directory beside the executable, one directory above the executable, and from the working directory. For standard `build-gcc/`, `build-clang/`, and `LINUX/` layouts the pack stays at the project root.

**Rights/provenance:** These panels are converted from prototype-era *Alien vs Predator* artwork and not original independently licensed media. Their per-file source attribution and checksums are in `PROVENANCE.json`. No blanket license or permission to publicly redistribute the artwork is asserted. Consult the repository's `LICENSE.md` and `PROVENANCE.md`. No ROM data is included.
