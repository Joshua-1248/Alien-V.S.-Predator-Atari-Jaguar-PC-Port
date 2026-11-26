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

## Features at a glance

The native port runs reconstructed Jaguar gameplay code on the PC rather than requiring a full-system emulator for ordinary gameplay. The original Marine, Alien and Predator campaigns remain the foundation. The PC extensions are optional where applicable: play with original-style visuals and rules, or enable the modern rendering, controls, gameplay modifiers and multiplayer features you prefer.

- **Original campaigns:** all 15 logical levels, species-specific weapons, HUDs, vision modes, terminals, security clearance, lifts, ducts, airlocks, pickups and campaign progression.
- **Modern presentation:** configurable resolution, widescreen, smoother rendering, high-resolution assets, filtering, post-processing and optional experimental Hybrid Vulkan rendering.
- **Offline multiplayer:** local split-screen co-op and Deathmatch, up to four local player slots and 32 additional AI bots, species selection, configurable bot behavior and match rules.
- **Customization:** configurable enemy statistics, population modifiers, cheats, audio, controls, and the integrated Map Maker.

Implementation does not imply complete original-hardware fidelity or that every combination of options has been validated. See [feature and verification status](docs/FEATURE_STATUS.md) and [R502 release notes](docs/RELEASE_NOTES_R502.md).

## Menus and controls

Press **F12** to open the PC options menu. Use the arrow keys to navigate and adjust options, Enter to select or bind an action, and Esc/Backspace to return. The original game menus and species-specific gameplay controls are retained alongside PC controls.

| Menu | Purpose |
| --- | --- |
| Controls | Keyboard, mouse, gamepad/joystick, Jaguar controller mappings and local-player bindings. |
| Video | Renderer, GPU selection, display modes, resolution, aspect ratio, presentation rate, filtering, post-processing and stereo preview. |
| Audio | Speaker layout, subwoofer channels, volume and music/ambience controls. |
| Enhancements | Optional visual, performance, audio, movement and gameplay improvements. |
| Cheats | Original cheat menus and additional PC cheats. |
| Debug | Refresh the current level. |
| Miscellaneous | Quicksave/load, restart, local multiplayer toggle and additional options. |
| Map Maker | Integrated map editor. |
| Player Options | Local multiplayer player options. |
| Modifiers | Enemy counts, per-enemy statistics and Facehugger modifiers. |

**F2** opens local multiplayer setup from the title screen once Local Multiplayer is enabled in F12 > Miscellaneous. **F5** quicksaves; **F9** quickloads where supported. Key and device assignments can be customized, including previous/next weapon, Alien claw/bite/tail, Predator medkit/cloak/vision, terminal controls, map controls and additional quick-turn bindings.

Mouse sensitivity and acceleration, analog movement, turn sensitivity/speed, axis inversion, individual gamepad deadzones, short button/analog input buffering and enhanced analog controls are available. Local players have separate input profiles. Physical device availability and access depend on the operating system; not every controller or multi-mouse configuration has been tested.

## Graphics and presentation

### Video settings

- Software rendering is the normal supported build; the opt-in **Hybrid Vulkan** path is experimental and includes hardware GPU selection. See the Vulkan build instructions above. A Hardware selection alone is not proof that GPU rendering is active; the menu reports accepted GPU frames where supported.
- Windowed, borderless and fullscreen display modes; selectable window dimensions and fullscreen refresh rate; 4:3, 16:9, 16:10, 21:9 and 32:9 aspect ratios.
- Configurable PC presentation rate, V-Sync (when available), FPS counter, and fog disabling.
- **Smooth Presentation** interpolates host-side movement, turning and visual effects, including damage-screen presentation, without intentionally speeding up the original 10 Hz gameplay simulation.
- **Window-resolution 3D** renders world geometry at the window's resolution rather than simply enlarging the original low-resolution image.
- Nearest (Jaguar), nearest-mipmap and bilinear texture filtering, anisotropic filtering, and independent filtering controls for HUD, enemies, player weapons, geometry and props.
- Extend draw distance and fog distance; optional fixed HUD hue independent of the background; optional Game Micro-Optimization for supported build paths.

### Post-processing and stereo preview

F12 > Video > Post-Processing provides a configurable vignette and scanlines. Scanlines have 12 selectable patterns: horizontal, vertical, grid, rising/falling diagonals and diagonal grid, with hard and soft variants. Adjust size, spacing, intensity and offset independently, or reset to the default settings.

The VR Headset menu provides an experimental **side-by-side stereoscopic preview**, eye separation, zero-parallax distance, eye-frame aspect, preview yaw, recentering and stereo screenshot output. It is not a complete tracked VR/OpenXR implementation.

### High-resolution HUD and vision enhancements

- High-resolution automap, motion tracker blips and Predator Audio Waveform Analyzer (AWA), including adjustable AWA detail (2x, 4x or 8x).
- Remember explored automap areas; fill automap hatch gaps and corners.
- Enhanced Predator Electromagnetic, Thermal and Infrared vision modes and an enhanced background option for Predator vision.
- Original-style display remains selectable; these options do not require an external texture pack.

## High-resolution textures and sprites

Enable **F12 > Enhancements > Hi-Res Textures** to load optional external artwork. The feature supports more than wall textures: floors, ceilings, doors, signage components, props, enemies, first-person weapon frames and HUD backgrounds have replacement paths. Assets retain their original ROM identifiers and frame mappings; this is not a 3D model replacement system.

Place the asset folders **beside the actual game executable**, not just in the source directory or current working directory. For example, when running `build-gcc/avp_pc`, put the packs inside `build-gcc/`. For packaged releases use the executable's `LINUX/` directory.

| Directory | Naming convention | Supported content |
| --- | --- | --- |
| `hi_res_textures/` | `00.png`–`73.png` | 74 FILES.DAT environment/signage slots, including panels and overlays. |
| `hi_res_props/` | `SS_FFF.png` (e.g. `03_000.png`) | Prop AAF sheet and frame identifiers. |
| `hi_res_enemies/` | `SS_FFF.png` | Enemy AAF sheet and frame identifiers. |
| `hi_res_weapons/` | `133.png`–`184.png` | First-person weapon FILES.DAT frames. |
| `hi_res_hud/` | `185.png`–`187.png` | Species HUD background images. |

`SS` is the two-digit original AAF sheet ID (00–62); `FFF` is the zero-padded three-digit frame number, limited to existing frames in the source sheet. Weapon/HUD names use the original decimal FILES.DAT record number. Supported filenames are slots, not a promise that replacement artwork exists for every slot.

Use non-interlaced 8-bit RGB/RGBA PNGs (maximum 4096 pixels per dimension); props, enemies, weapons and HUD replacements require RGBA with an alpha channel. Preserve the original layout, proportions, frame alignment and transparency when creating replacements. High-resolution signs are assembled from the original signage components using the retail layout. The loader validates PNGs and applies decoded-memory limits; incomplete packs are supported. Missing or invalid replacements fall back individually to original artwork, or enabled prototype panels where applicable.

The console reports loaded panels, signs, props, enemies, weapons and HUD backgrounds. Hi-res textures are optional and the game still requires the retail ROM. The repository contains the existing environment texture pack; additional sprite packs are optional and are not guaranteed to be included or complete. No claim is made that every renderer and every replacement combination has been fully tested.

## Prototype Texture Restoration

The earlier Jaguar prototypes contain **higher-quality versions of selected textures than the final retail release**. Many preserve finer material detail, clearer patterns and less visibly degraded artwork. This is a restoration of earlier game artwork, not AI upscaling or a recreation of the prototypes' gameplay. Individual textures can also differ artistically, and the earlier date does not make every image automatically superior or lossless.

Enable **F12 > Enhancements > Prototype Texture Restoration** to substitute up to 51 mapped panels while retaining retail maps, geometry and gameplay. Source priority is October 1993, August 18, 1993, April 8, 1994, then retail. Missing panels fall back to retail. The restoration is independent of the high-resolution replacement option: supplied high-resolution panels take precedence, with enabled prototype panels used where no suitable hi-res replacement exists.

See [Prototype Texture Restoration — installation, source priority and verification](README_S105_PROTOTYPE_TEXTURE_RESTORATION.md). A local restoration pack uses `.prgb` panels (legacy `.rgb` supported), not the PNG naming convention used by the high-resolution pack. Use the included generator with your own prototype archives if you need to recreate the pack.

## Audio

The host audio system reconstructs Jaguar FullSynth/JERRY sound playback, with procedural JERRY waveforms; no external JERRY wavetable or Jaguar BIOS ROM is required.

- Speaker layout selection, subwoofer channel configuration and active output channel status.
- Independent music, sound effects, ambience, ambient hissing noise and voice volume; music/ambience/hissing toggles.
- Optional motion tracker sounds, player pain sounds, facehugged pain and Facehugger shake pain sounds.
- **Sound Effect Channels:** original eight channels by default, selectable in increments of eight up to 256. This controls concurrent sound effects, not the number of physical speakers.
- Local multiplayer spatial/proximity audio support; device-specific channel output and exact physical Jaguar audio parity remain subject to testing.

## Gameplay enhancements and modifiers

### Optional gameplay enhancements

These options are separate from the original campaign rules and can be used to customize play:

- Extend enemy activation distance (original, 2x, 3x, 4x or unlimited); unlimited shooting range.
- Marine line-of-sight hits and Marine fire interruption; optional Marine shotgun/full-ammo start.
- Normalize diagonal contact range, Alien death spray range and enemy movement speed.
- Persistent enemy and hazard health; configurable generator blast radius; crawling Alien death interruption.
- Predator cloak honor options (no lost honor points or half-value cloaked kills) and melee attacks opening jammed doors.
- Player melee attacks or all player attacks aimed at screen center; optional Alien cocoon-combo preservation when infinite ammunition is enabled.

### Enemy modifiers

F12 > Modifiers includes an enemy population multiplier (1x–4x, applied on first visit) and independent damage, health and movement-speed multipliers (0.25x–4x in quarter-step increments) for Alien, Crawling Alien, Facehugger, Egg, Marine, Predator and Alien Queen. Health multipliers are applied at spawn/reload. Individual modifiers may have species-specific limitations.

- **Marine Vent Facehuggers:** optional extra Facehuggers in 4B (3), 4C (3), 5B (5) and 5C (5), for a fixed total of 16 additional Facehuggers. No extras are added to the other vents; original placements remain unchanged. Previously visited saved levels keep their stored populations.
- **Facehugger Instant Death:** optional alternate Facehugger difficulty behavior.

### Cheats and debugging

Original Hold That Fuji, Banana Stops and Global Positioning options are retained, alongside PC cheats: restore health/ammo, infinite health/ammo, invincibility (separate from healing), give weapons, motion tracker, security clearance increase/decrease, noclip, freeze enemies, prevent attacks or Facehugger attachment, kill selected enemy types or all enemies, clear enemies/hazards, reveal the entire map, show enemies on map, movement speed (1x–4x), level selection and debug refresh. Cheat availability depends on species and mode. Quicksave/load and restart are also available; not every multiplayer save combination is supported.

## Offline local multiplayer and bots

Enable Local Multiplayer in F12 > Miscellaneous, then open F2 from the title screen. Choose **Co-op** or **Deathmatch**, player count, characters, bots, levels and match options. **No online multiplayer, public servers or matchmaking are implemented.**

- Up to four local player slots and up to 32 additional bots (36 total actor slots); two-player side-by-side/top-bottom and three-player layout options; separate player views, HUDs, controls and configurable AI control of player slots.
- Marine, Alien, Predator or random character selection, configurable bot species and Balanced, Berserker or Guerilla behavior profiles.
- Deathmatch score and time limits, scoreboard, respawning, selected/random starting and respawn levels, optional NPC enemies, player collisions, cross-level travel, random character change on death, automap player visibility and whole-map options.
- Optional fast movement, fast equip, Marine shotgun/full-ammo start, cloak damage and uncloaked Predator PvP damage modifiers.
- Spycam with player selection and next/previous-player bindings; local multiplayer preference persistence; custom-map selection.

### Bot AI (active development)

Bots have species-aware combat and weapon decisions, ammo/weapon acquisition, Marine corpse scavenging, health and medical-terminal objectives, route planning, lift/duct/airlock interaction logic, player and NPC collision avoidance, acid avoidance, and recovery from blocked routes or repeated movement loops.

R502 improves Predator decloak/attack decisions: bots request decloak before attacking, with a bounded fallback if the cloak state persists. Acid-vulnerable bots prefer dry routes, but can commit to a necessary crossing of unavoidable vent acid, including bends and return journeys. They still receive normal acid damage. The AI remains experimental: prolonged stalls, survival, weapon acquisition, interaction completion and the exact reported acid loop remain open issues. Co-op spawn behavior is unchanged in this revision.

## Map Maker

Open Map Maker from F12. Edit maps using the integrated 2D overhead editor and 3D preview: cell, floor/ceiling, wall, door and object editing; texture browser and original sprite thumbnails; painting, selection, copy/paste and undo/redo; guarded script/panel editing, draft saving/loading and playtesting where supported. Custom maps use `.avpm` files rather than modifying the cartridge ROM. The editor supports retail-map research and custom-map workflows, but not every retail scripted interaction can be relocated or authored freely.

See [Map Maker documentation](new/map_maker/README.md) and [map format and all 15 levels](new/map_maker/MAP_FORMAT_AND_15_LEVELS.md). These documents include historical implementation notes; not all historical limitations describe the current build.

## Verification

R502 release tests: GCC and Clang software builds and Vulkan builds completed; the registered software CTest suite passed 238/238 with both compilers. Native retail-map fixtures passed 512 vent return journeys and 64 corner cases. Three retail Deathmatch smoke tests exercised Predator combat, but did not reproduce the reported acid loop or naturally trigger the stuck-cloak fallback. These tests do not establish complete AI or hardware fidelity.

```sh
python3 new/source_integrity/check_game_source.py
cmake --build build-gcc -j4
ctest --test-dir build-gcc --output-on-failure
```

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/`, `include/` | Protected original reconstructed source and compatibility-facing headers. |
| `new/` | PC platform/render/audio code, extensions, bots, modifiers, Map Maker, compatibility overlay and source integrity tooling. |
| `third_party/` | Third-party dependencies and notices. |
| `tests/`, `tools/` | Regression tests and developer/verification tools. |
| `docs/` | Project documentation, verification and legal notices. |
| `hi_res_textures/`, `prototype_texture_restoration/` | Optional artwork packs; do not add artwork without distribution rights. |

Build outputs, ROMs, generated caches, personal settings, screenshots and historical log dumps must not be committed.

## Rights and attribution

Original Atari Jaguar game code/assets are subject to their respective rights holders. This source repository is not a license to redistribute the commercial cartridge ROM. Read [`LICENSE.md`](LICENSE.md) and the [credits, provenance and third-party notices](docs/legal/). The project is not endorsed by Atari or the original developers.

## Source ownership (RE49 R397)

The original reconstructed source files are now byte-identical to the archived C decompilation reference. New code and explicitly recorded inherited compatibility deltas are under `new/`. CMake verifies these originals before generating a disposable build-only source view; do not edit that view. See [source ownership and build details](new/port_overlay/README.md).
