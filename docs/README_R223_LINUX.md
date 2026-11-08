# Alien vs Predator (1994 Atari Jaguar) — RE #21 / R223 Linux Mint

R223 is a **research-only** original-VBlank/OP-list audit based on R222. The port's game code, menus, graphics, audio, controls, and original 10-Hz simulation remain unchanged. GCC and Clang executables are byte-identical to R222. Original hardware frame-accurate transition parity is not yet claimed.

The full archive includes current full source (original LICENSE/CREDITS/PROVENANCE/THIRD_PARTY_NOTICES), original project cumulative R1–R223 addendum, R223 verification and audit, and native x86_64 Linux GCC and Clang executables with Unix executable permission `0755`.

Place your own legally owned unmodified `AVP.jag` next to the executable, or launch:

```bash
./avp_pc --rom /absolute/path/to/AVP.jag
```

Original retail ROM SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`.

Build with no more than four jobs:

```bash
cd SOURCE_R223
cmake -S . -B build-gcc -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc -j4
cmake -S . -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang -j4
```

Read-only cartridge research audit:

```bash
python3 tools/audit_retail_prelist_vblank_dispatch.py --rom /absolute/path/to/AVP.jag
```

See `R223_PRELIST_VBLANK_INTERRUPT_AUDIT.md` for the actual findings and remaining uncertainties. No commercial ROM/media, caches, saves, previous releases or diagnostic production modifications are included.
