# Alien vs Predator (Atari Jaguar) — Native PC Port

Unofficial work-in-progress native PC port of the 1994 Atari Jaguar game. This repository contains reconstructed and independently written program code. It **does not** contain the original game ROM or require a separate Jaguar BIOS or JERRY wavetable ROM.

**Current development source: RE #52 R502 (November 26, 2026).** The original retail game is the behavioral reference, but full 1:1 hardware fidelity and every multiplayer/editor interaction are **not** yet certified.

## Requirements

- Your own retail Atari Jaguar `AVP.jag` (4 MiB; SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`).
- Linux with X11, a C toolchain (GCC or Clang), CMake, Ninja, Python 3.8+, and platform build dependencies.
- **No other ROM** is required. JERRY waveforms are generated procedurally.

## Linux build

On Debian/Ubuntu/Linux Mint:

```sh
sudo apt update
sudo apt install -y build-essential clang cmake ninja-build python3 libx11-dev libxext-dev libxrandr-dev zlib1g-dev libssl-dev

cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=ON
cmake --build build-gcc --target avp_pc -j4
./build-gcc/avp_pc --rom /path/to/AVP.jag

CC=clang CXX=clang++ cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=ON
cmake --build build-clang --target avp_pc -j4
./build-clang/avp_pc --rom /path/to/AVP.jag
```

The source builds `avp_pc` with either compiler. Binary releases identify the second as `avp_pc_clang` and place **both** in one `LINUX/` folder.

## Optional Vulkan build (Linux/X11)

With the Vulkan SDK and shader compiler installed and its environment configured:

```sh
cmake -S . -B build-vulkan -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=ON \
  -DAVP_BUILD_VULKAN_WSI_DIAGNOSTIC=ON \
  -DAVP_BUILD_VULKAN_SHADER_DIAGNOSTIC=ON \
  -DAVP_BUILD_VULKAN_GAME_PROBE=ON
cmake --build build-vulkan --target avp_pc -j4
./build-vulkan/avp_pc --rom /path/to/AVP.jag
```

Hardware rendering is experimental; R502 was compiled with Vulkan but not validated on physical Vulkan hardware. The normal build uses the software renderer. See [Windows build instructions](WINDOWS_BUILD.md) for the separate Windows path; R502 Windows runtime was not verified.

## Features

- Reconstructed Jaguar campaign logic and source-backed title/frontend.
- Local split-screen multiplayer (co-op and deathmatch), local AI bots and their behavior options. **Online multiplayer is not part of this port.**
- Map Maker, per-player controls, cheats, configurable enhancements, and saveable local multiplayer preferences.
- PC audio/video presentation with procedural JERRY sound generation; only the original game cartridge ROM supplies the game content.

- Optional high-resolution texture/sprite support, smooth presentation, configurable sound channels, post-processing and an experimental Vulkan path.
- R502 improves Predator decloak/attack decisions and permits committed crossings of unavoidable acid in vents, with dry routes preferred. Multiplayer AI remains experimental, not fully solved.

See [R502 release notes](docs/RELEASE_NOTES_R502.md) and [feature and verification status](docs/FEATURE_STATUS.md).

## Verification

R502 release tests: GCC and Clang software builds and Vulkan builds completed; the registered software CTest suite passed 238/238 with both compilers. Native retail-map fixtures passed 512 vent return journeys and 64 corner cases. Three retail Deathmatch smoke tests exercised Predator combat, but did not reproduce the reported acid loop or naturally trigger the stuck-cloak fallback. These tests do not establish complete AI or hardware fidelity.

```sh
python3 new/source_integrity/check_game_source.py
cmake --build build-gcc -j4
ctest --test-dir build-gcc --output-on-failure
```

## Layout

`src/`, `include/`, and `new/` contain program code; `third_party/` has required dependencies; `tests/` holds regressions; `tools/` holds developer scripts; `docs/` holds documentation and legal notices. Build outputs, ROMs, cached assets, screenshots, and historical log dumps must not be committed.

## Optional texture packs

Texture replacements are optional; the retail ROM is still required. Existing high-resolution packs may be placed in `hi_res_textures/`. See the [prototype texture restoration instructions](README_S105_PROTOTYPE_TEXTURE_RESTORATION.md) for local pack generation. Do not commit ROMs, extracted retail assets, caches or personal settings. Only distribute artwork you have rights to distribute.

## Rights and attribution

Original Atari Jaguar game code/assets are subject to their respective rights holders. This source repository is not a license to redistribute the commercial cartridge ROM. Read [`LICENSE.md`](LICENSE.md) and the [credits, provenance and third-party notices](docs/legal/). The project is not endorsed by Atari or the original developers.

## Source ownership (RE49 R397)

The original reconstructed source files are now byte-identical to the archived C decompilation reference. New code and explicitly recorded inherited compatibility deltas are under `new/`. CMake verifies these originals before generating a disposable build-only source view; do not edit that view. See [source ownership and build details](new/port_overlay/README.md).
