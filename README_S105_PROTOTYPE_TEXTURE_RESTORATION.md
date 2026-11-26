# Prototype Texture Restoration (S106; compatible with S105)

**F12 > Enhancements > Prototype Texture Restoration** is OFF by default.
When enabled, the host renderer replaces up to 51 confirmed candidate retail wall/floor texture slots with PNG source pixels extracted locally from the user's historical Jaguar prototype archives. It does not modify game logic, map texture references or the original ROM.

## Prepare your texture pack (Linux / extracted folders)

Place the three archives you already own in a convenient location, then run:

```bash
python3 tools/create_prototype_texture_pack.py \
  --oct '/path/to/Oct 1993.7z' \
  --aug '/path/to/1993-08-18.7z' \
  --apr '/path/to/1994-04-08.7z' \
  --output './prototype_texture_restoration'
```

The script needs Python 3 and the system's `libarchive` to read 7z archives. Alternatively extract them with your normal archive tool and supply their directory paths. It has no pip or Pillow dependency.

Run `avp_pc` (or the GCC/Clang executable) from this same directory. Open F12 > Enhancements and enable the feature. The menu shows the number of loaded replacements (usually 51). Missing/malformed entries fall back to retail textures. To locate a pack elsewhere, set `AVP_PROTOTYPE_TEXTURE_DIR` to the path before launching.

The pack is your private data derived from your legally supplied archives; do not redistribute it. The distributed source, Linux executables and patch do not include those images, the retail ROM or any extracted sprites.

### Priority and coverage

October 1993 > August 18, 1993 > April 8, 1994 > retail. The mapping is intentionally conservative: 5 Oct, 27 Aug, 19 April replacements, with 5 retail fallbacks among FILES 0..55. See `tools/prototype_texture_restoration_manifest.json` and `RE28_S105_PROTOTYPE_TEXTURE_RESTORATION_VERIFICATION.md` for exact IDs and confidence. These PNG exports are decoded RGB artwork, not proof of a lossless original source: names such as `JP90` imply some original artwork was still compressed.

The original retail ROM is still required. A missing pack is safe; enabling the toggle then displays `Enabled (0 panels)` without modifying graphics. Disabling restores the exact retail texture buffers. This affects single-player and all co-op viewports simultaneously.

### S106 pack integrity and backward compatibility

S106's local pack generator verifies each selected prototype PNG against the source
manifest SHA-256 **before writing** any replacements. It produces `.prgb` panels
with a 16-byte `AVPTEX1` header, uncompressed RGB888 payload, and CRC-32.
The renderer rejects a corrupted or truncated framed image and uses its retail
counterpart. Previous S105 `NN.rgb` packs are still accepted when no corresponding
`NN.prgb` exists; legacy files did not carry checksums.

Re-running the generator updates the pack and removes only obsolete numbered
S105 `.rgb` entries (00 through 55) and stale numbered `.prgb` entries, leaving
unrelated user files untouched. An invalid input PNG causes the generator to
exit before changing the existing pack. The pack remains private and generated
locally from your own prototype archives.

The 51 candidate mappings have **not** been changed; frame integrity is not
proof that an earlier prototype image is sharper or closer to the final art.
Sprited graphics and other image families remain outside this environment-panel
feature.

To verify a generated pack without reading the original archives again:

```bash
python3 tools/verify_prototype_texture_pack.py prototype_texture_restoration
```

This checks not just the internal CRC but also the chosen retail-slot mapping and
source RGB SHA-256. It accepts only the 51 S106 verified candidates; previous
S105 raw packs should be regenerated to use the new verification tool.
