# Alien vs Predator (1994 Atari Jaguar) — RE #21 / R225 Linux Mint

R225 is a **research-only release**. It preserves R224 gameplay/render/audio/input/save code. The original displayed transition is not yet proven 1:1. Full source, GNU GCC and LLVM Clang x86_64 executables, updated cumulative R1–R225 history and all original license/credits are included.

Use a legally owned canonical `AVP.jag` alongside either `avp_pc` executable, or start with:

```bash
./avp_pc --rom /absolute/path/to/AVP.jag
```

Expected retail ROM SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`.

Build from this current source, up to four parallel compiler jobs:

```bash
cd SOURCE_R225
cmake -S . -B build-gcc -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc -j4
cmake -S . -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang -j4
```

Read-only offline cartridge source audit (private ROM required; no image or sound extraction):

```bash
python3 tools/audit_retail_frontend_vblank_callback_lifetimes.py --rom /absolute/path/to/AVP.jag
```

See `R225_RETAIL_VBLANK_CALLBACK_AUDIT.md`. No gameplay changes, host transition tweaks, or diagnostic patches were introduced.
