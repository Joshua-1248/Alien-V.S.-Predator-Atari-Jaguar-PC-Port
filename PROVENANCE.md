# Provenance

## Primary sources

1. Canonical World retail ROM behavior and the byte-exact reconstruction/oracle repository.
2. The surviving near-final September 1994 Jaguar assembly source tree (`MAIN`, `MAZE`, `AMP`, etc.) used as historical reference where available.
3. Reconstructed missing modules (`OBJECTS`, `MJP`, Jaguar support, FILES, MESSAGES, AVPSOUND, UNZIP) from the preservation project.
4. Prototype builds used only as comparative evidence where retail semantics are ambiguous.

## C conversion rule

A C function should express the established runtime semantics, not mimic register allocation. Address/bit-width/order-sensitive behavior is documented in comments when it matters. The exact preservation repository remains the binary oracle.

## Distribution rule

No retail/prototype ROM images, exact retail binary function slices, or proprietary third-party historical source files are included. **Exception for optional PC presentation:** `prototype_texture_restoration/` contains 51 converted RGB panels taken from prototype-era *Alien vs Predator* artwork, recorded by source tier, member filename and checksum in `prototype_texture_restoration/PROVENANCE.json`. These are not independent original artwork and remain subject to the underlying rights. The format conversion, checksums, and this documentation do not establish permission for public redistribution.

The runtime still requires an external, user-supplied compatible 4 MiB retail ROM for retail game assets.

## M2.0 presentation backport

M2.0 reuses independently reconstructed host-side JagPEG decoding and panel-selection logic from the project's earlier Native Port Milestone 20. It is used only as a PC presentation/resource layer; gameplay remains driven by the later RE #7 readable C reconstruction.
