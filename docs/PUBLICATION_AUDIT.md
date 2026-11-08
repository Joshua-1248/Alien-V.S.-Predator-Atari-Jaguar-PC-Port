# Source publication checklist — RE33 R71

Prepared from the R71 full source archive with an allowlisted code structure, without changing implementation code. The clean source repository retains the actual C/C++ source, CMake configuration, test suite, compiler helpers, platform build tools and original third-party license notices.

**Intentionally excluded:** the user's game ROM(s), prototype ROMs, extracted retail assets except as otherwise noted, game audio/graphics runtime caches, built executables or archive bundles, settings/saves, private code/oracle captures, duplicate cumulative project addenda, historical compiler logs, and old patch dumps. These may exist in private research/release archives, but are not Git source dependencies.

**Explicit R71 texture enhancement exception:** `prototype_texture_restoration/` now includes **51 converted prototype-art RGB panels** in `.prgb` format, as supplied by the maintainer, plus their `PROVENANCE.json`. The pack was validated against `tools/prototype_texture_restoration_manifest.json`, CRC32 and SHA-256 mappings, and exercised by the original C texture loader. These are derived from copyrighted game artwork: file format conversion and source attribution do not establish public-redistribution rights. No compatible retail ROM is included.

**Legal boundary:** the game-derived readable-C reconstruction is still based on copyrighted material; including disclaimers is not equivalent to obtaining copyright permission. No blanket project license has been asserted, and there is no guarantee that publication of every reconstructed code portion is legally authorized. Obtain advice or rights-holder permission where necessary.

**Verification boundary:** automated regression tests and past successful GCC/Clang releases support confidence, but live gameplay parity, Linux Mint host/controller combinations and current Windows hardware are not fully established. Read `docs/FEATURE_STATUS.md`.
