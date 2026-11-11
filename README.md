# Alien vs Predator (Atari Jaguar) — Native PC Port

Unofficial work-in-progress native PC port of the 1994 Atari Jaguar game. This repository contains reconstructed and independently written program code. It **does not** contain the original game ROM or require a separate Jaguar BIOS or JERRY wavetable ROM.

**Current source: RE #36 R116.** The original retail game is the behavioral reference, but full 1:1 hardware fidelity and every multiplayer/editor interaction are **not** yet certified.

## Requirements

- Your own retail Atari Jaguar `AVP.jag` (4 MiB; SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`).
- Linux with X11, a C toolchain (GCC or Clang), CMake, Ninja, and platform build dependencies.
- **No other ROM** is required. JERRY waveforms are generated procedurally.

## Linux build

On Debian/Ubuntu/Linux Mint:

```sh
sudo apt update
sudo apt install -y build-essential clang cmake ninja-build libx11-dev libxext-dev libxrandr-dev zlib1g-dev libssl-dev

cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=ON
cmake --build build-gcc --target avp_pc -j4
./build-gcc/avp_pc --rom /path/to/AVP.jag

CC=clang CXX=clang++ cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=ON
cmake --build build-clang --target avp_pc -j4
./build-clang/avp_pc --rom /path/to/AVP.jag
```

The source builds `avp_pc` with either compiler. Binary releases identify the second as `avp_pc_clang` and place **both** in one `LINUX/` folder.

## Features

- Reconstructed Jaguar campaign logic and source-backed title/frontend.
- Local split-screen multiplayer (co-op and deathmatch), local AI bots and their behavior options. **Online multiplayer is not part of this port.**
- Map Maker, per-player controls, cheats, configurable enhancements, and saveable local multiplayer preferences.
- PC audio/video presentation with procedural JERRY sound generation; only the original game cartridge ROM supplies the game content.

R116 improves the Marine bot's short-term Alien-luring retreat in narrow corridors; other source systems remain preserved. For important known limitations, see [`docs/FEATURE_STATUS.md`](docs/FEATURE_STATUS.md).

## Layout

`src/`, `include/`, and `new/` contain program code; `third_party/` has required dependencies; `tests/` holds regressions; `tools/` holds developer scripts; `docs/` holds documentation and legal notices. Build outputs, ROMs, cached assets, screenshots, and historical log dumps must not be committed.

## Rights and attribution

Original Atari Jaguar game code/assets are subject to their respective rights holders. This source repository is not a license to redistribute the commercial cartridge ROM. Read [`LICENSE.md`](LICENSE.md) and the [credits, provenance and third-party notices](docs/legal/). The project is not endorsed by Atari or the original developers.
