# Alien vs Predator (1994 Atari Jaguar) — RE #21 / R224 Linux Mint

R224 is **research only**, preserving R223 game/runtime code and the original 10-Hz simulation. It includes the original Jaguar BRANCHOBJ packing and VC aperture audit, cumulative history, GCC and Clang x86_64 executables, full source, original project credits and licensing. The PC transition remains unverified at pixel- and cycle-level.

Place a legally owned canonical `AVP.jag` alongside either `avp_pc` binary or launch with:

```bash
./avp_pc --rom /absolute/path/to/AVP.jag
```

Original cartridge SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`.

Independent builds (four jobs):

```bash
cd SOURCE_R224
cmake -S . -B build-gcc -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc -j4
cmake -S . -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang -j4
```

Run ROM research audit (private, no captured ROM content):

```bash
python3 tools/audit_retail_prelist_branch_conditions.py --rom /absolute/path/to/AVP.jag
```

See `R224_PRELIST_BRANCH_OBJECT_CONDITIONS.md` for proof and remaining unknowns. This source-only checker does not alter game output or create diagnostic builds.
