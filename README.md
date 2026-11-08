# Alien vs Predator (Atari Jaguar) — Native PC Port

**Unofficial, community-maintained reverse-engineering project · Work in progress · Source baseline: RE #33 R71 (November 2026)**

This project aims to bring **Alien vs Predator (1994) for Atari Jaguar** to modern PCs while preserving the original game's campaign, controls, logic, artwork, sound and feel. It uses a readable-C reconstruction of the Jaguar's original gameplay logic, new PC platform/render/audio support, and optional PC-only features such as offline local multiplayer, bots, Deathmatch and a map editor.

**Important status warning:** This is an active development project, **not a declared 100%-complete or bug-free port**. Many features have passed automated regression tests; others require hands-on verification against original Jaguar hardware or a retail ROM. “Implemented” below means present in the current source—not necessarily exhaustively tested in every scenario. See [Feature and verification status](docs/FEATURE_STATUS.md) and [Known limitations](#known-limitations-and-unverified-behavior).

**The original retail game ROM is not included.** To play the retail game, supply your own compatible copy of the original Atari Jaguar cartridge ROM. This repository additionally includes an optional pack of 51 converted prototype texture panels under `prototype_texture_restoration/`; see [Prototype texture restoration](#prototype-texture-restoration). These converted images are derived from game artwork, and their inclusion does not constitute permission to redistribute the original artwork. This project is not affiliated with or endorsed by Atari, Rebellion, the game's other rights holders, or any platform manufacturer.

## Quick links

- [Install, build and run](#build-and-run-on-linux)
- [Windows status](#windows)
- [Menus and controls](#menus-and-controls)
- [Single-player and local multiplayer](#game-modes)
- [Bots](#bots)
- [Cheats](#cheats-and-debugging)
- [Enhancements](#optional-enhancements)
- [Map Maker](#map-maker)
- [What is and is not finished](#known-limitations-and-unverified-behavior)
- [Acknowledgements](#acknowledgements)
- [Licensing, rights, credits](#copyright-licensing-and-provenance)
- [Source layout](#repository-layout)

## Requirements and original game ROM

- **Primary development environment:** Linux/X11, including Linux Mint and Ubuntu. GCC and Clang are used to build the native code.
- **Windows:** A Win32/WinMM/XInput implementation and cross-build workflow exist; see [Windows status](#windows). The latest R71-specific Windows build/runtime has **not** been independently verified.
- **Retail game ROM:** canonical **4 MiB Atari Jaguar retail-world `AVP.jag`**, SHA-256:

  `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`

The retail port validates the ROM hash at launch. Alpha and beta ROMs are **not** interchangeable with the expected retail data. No Jaguar BIOS, boot ROM or `jagwave.rom` is needed for the normal native PC path. Resource data is decoded from the supplied retail ROM, and the port may generate **local** caches, settings and save files. **Do not commit retail ROMs or generated caches.** The optional prototype-derived texture pack is identified separately below; no retail ROM is supplied.

You can verify your ROM on Linux:

```bash
sha256sum "/path/to/AVP.jag"
```

## Build and run on Linux

Install typical dependencies (Debian / Ubuntu / Linux Mint):

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
  libx11-dev libxext-dev libxrandr-dev libssl-dev zlib1g-dev
```

From the **repository root**:

```bash
cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAVP_STRICT=ON -DAVP_BUILD_LOCAL_MULTIPLAYER=ON \
  -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-gcc --target avp_pc -j 4
./build-gcc/avp_pc --rom "/path/to/AVP.jag"
```

Clang alternative:

```bash
sudo apt install -y clang
CC=clang CXX=clang++ cmake -S . -B build-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON \
  -DAVP_BUILD_LOCAL_MULTIPLAYER=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-clang --target avp_pc -j 4
./build-clang/avp_pc --rom "/path/to/AVP.jag"
```

The build output is named **`avp_pc`** for both compilers. Historical downloadable Linux packages instead distinguish the precompiled GCC build as `LINUX/avp_pc` and the Clang build as `LINUX/avp_pc_clang`; these binaries are **not committed to this source repository**. You can rename your locally compiled Clang copy to `avp_pc_clang` if desired. Linux executables must have execute permission (if necessary, `chmod +x avp_pc`).

The executable can also discover a supported `.jag` next to itself, but **explicit `--rom` is recommended**. Run `./build-gcc/avp_pc --help` for available CLI options. `--diagnostic` launches a diagnostic geometry path and **is not a substitute for the retail game ROM**.

**Testing:** Most source tests build as separate CMake targets. The repository contains `tests/run_tests.sh` and `tools/validate.sh`; these can be resource-intensive and some retail reference comparisons require your own ROM. At minimum, build both compilers and run the relevant regression executables for changes. Never treat a successful compile as proof that every level or device works flawlessly.

## Windows

The source contains a native Windows backend (Win32/GDI rendering, WinMM audio and XInput) and scripts under `tools/`, plus an existing GitHub Actions workflow. Follow [`WINDOWS_BUILD.md`](WINDOWS_BUILD.md) for MSYS2 UCRT64 or MinGW-w64 instructions. Its historical build results **do not certify R71 on a physical Windows machine**. In particular, real Windows audio, controller hardware, full-screen behavior, and the newest multiplayer features may need additional testing. Windows persistent quickstate handling differs from Linux; see the Windows guide.

## Gameplay and fidelity goals

- **Original campaigns:** Marine, Alien and Predator story paths, with source-driven movement, enemies, attack logic, pickups, level transitions, terminals and inventory.
- **Original timing:** Gameplay simulation follows the source-established 10 Hz tick. PC presentation can run at higher refresh rates without intentionally accelerating the original simulation.
- **Graphics and audio:** Original Jaguar ROM-backed textures, sprites, maps, fonts and sounds; software rendering, enhanced presentation options and reconstructed FullSynth audio. Some exact Jaguar GPU/DSP and audio characteristics remain under study.
- **Original behavior comes first:** PC enhancements are optional, and features with a fidelity-preserving Off/Original mode default to that mode where documented.

### Game modes

**Single-player** plays the original Jaguar campaigns. **Offline local multiplayer** supports 1–4 human players on one PC with split-screen layouts, cooperative gameplay and Deathmatch. The port supports player-specific controls, scores, respawns, camera/spy views and other multiplayer-only systems. Single-player fidelity and multiplayer extensions share the original game simulation where feasible.

**Deathmatch** includes player/level setup, optional limits, team/character settings where supported, player-vs-player weapon balancing, optional NPC enemy presence, body pickups and HUD kill messages. Deathmatch damage is intentionally distinct from the retail game's NPC damage: for example, as of R66+, a Marine flamethrower projectile deals **125 HP to players in Deathmatch**, not its original 10 HP to enemy actors. Behavior in complex edge cases still needs real-world verification.

**Offline only:** The multiplayer implementation is for local players and bots. It is **not an online multiplayer client/server, lobby, matchmaking or RCON project**. Do not assume remote online compatibility.

### Bots

- Up to **12 additional AI bots** alongside the configured human players, subject to game mode and performance.
- Alien, Predator, Marine or Random character selection; Balanced, Berserker and Guerilla behavior options.
- Navigation, obstacle avoidance, doors, elevators/vents and combat reactions have undergone repeated development and regression testing.
- Bot aiming, melee weapon reach, acid avoidance, awareness and pursuit are actively refined. **Bot AI is not claimed to be perfect**: corner cases and retail level interactions still require gameplay testing.
- In F2, **Number of Bots** is directly below Number of Players; additional bot settings live in the **Bots** submenu.

## Menus and controls

| Key/menu | What it does |
|---|---|
| **F12** | PC options: Audio, Video, Controls, Enhancements, Cheats, Debug, Miscellaneous, Map Maker and related settings |
| **F2** (title/menu) | Local multiplayer setup |
| **F2 → Bots** | AI-controlled player slots, bot characters and behaviors |
| **F2 → Modifiers** | Fast Movement, Fast Equip, random-character death switching, Predator cloak weakness |
| **F2 → Options** | Other-player automap dots, Show Whole Map, level movement and enemy presence |
| **Left/Right** | Adjust highlighted settings; for Cheats → Activate For, directly cycle Player 1–4 |
| **Enter** | Activate a PRESS ENTER action or enter a submenu |
| **Backspace / Escape** | Return to the previous F2 or F12 menu as appropriate |
| **F5 / F9** | PC quicksave / quickload where supported; platform restrictions apply |

P2–P4 have independent control bindings and controller options. The Jaguar logical pad (A/B/C, Pause, Option and keypad) can be mapped to keyboard/controller input. Configure bindings in the in-game F12 controls menus; **do not assume every controller uses the same default layout**. Some Map Maker keys are context-specific and differ from gameplay bindings.

### Audio

F12 Audio contains volume and on/off controls. **Ambience** controls the continuing level background sound, while **Ambient Hissing Noises** and **Ambient Hissing Noise Volume** independently control the game's intermittent random swishing/hissing events. Turning off one does not intentionally silence the other. Additional audio layout/speaker options are provided by the PC host; exact hardware parity remains a test area.

### Video and presentation

The PC host exposes display refresh/presentation, smooth display, fog and view-distance options, and FPS/V-Sync controls where supported by the driver and display. High presentation refresh does not change the authoritative 10 Hz gameplay tick. Behavior/performance may vary by GPU, monitor, split-screen layout, and host audio system.

## Cheats and debugging

F12 → **Cheats** contains **Original** (menu access to original Jaguar-era cheat systems), **New** (PC extension cheats), and **Activate For** (inline Player 1–4 target selector). Original cheats default Off. Their original controller/keypad code entry is also available for physical Player 1 logical Jaguar controls; P2–P4 use the menus for those actions.

- **Original → Hold That Fuji:** security clearance, tracker, Marine weapons and ammunition tools.
- **Original → Banana Stops:** expanded cheat controls, level selection and God Mode. God Mode and the original `CHEAT ON` / `CHEAT OFF` HUD notices are reproduced; save/load effects can differ when cheat state resets.
- **Original → Global Positioning:** decimal/fine hexadecimal X/Y coordinates and level identifier overlay for debugging.
- **New:** additional PC-only cheats such as Reveal Entire Map, **Clear Enemies From Level** and **Clear Hazards From Level**. Those clear actions remove actors without ordinary kill/death events and operate on the targeted player's current level, not every loaded level.

F12 → **Debug → Refresh Level** reinitializes the currently targeted level's original doors and actor population from the retail ROM while preserving the broader campaign/player progress. This is a debugging operation, **not** an ordinary save/load or campaign reset; advanced use may change the current level's state in surprising ways. Save important progress first.

## Optional enhancements

Available settings vary by build and context. Examples in R71 include:

| Enhancement | Purpose / limitations |
|---|---|
| **Normalize Diagonal Contact Range** | Opt-in circular contact-range calculation for certain harmful enemy contacts; Off retains the original Jaguar square-range rule. |
| **Extend Enemy Activation Distance** | **Original** (default), **2×, 3×, 4×, Unlimited**; extends the pursuit activation cutoff for enemies in **loaded/simulated levels**, not enemy attack range or unloaded levels. |
| **Extend Draw Distance** | PC rendering extension including extra and Unlimited distances; can cost performance. |
| **Extend Fog Distance / Disable Fog** | PC visual distance and fog options. |
| **Unlimited Shooting Range** | Optional shooting-range change; not a guarantee of a line of sight through walls. |
| **Smooth Presentation** | Interpolated PC visuals and multiplayer automap dots without changing simulation time. |
| **Game Micro-Optimization** | Optional PC code path intended to preserve gameplay behavior; performance improvements depend on system and scene. |

Some options are subject to multiplayer-specific conditions; each setting's original-versus-enhanced distinction matters when comparing with Jaguar hardware.

## Prototype texture restoration

The optional **Prototype Texture Restoration** enhancement is implemented and this repository update includes its **51 validated replacement texture panels** (`.prgb`) in `prototype_texture_restoration/`.

1. Compile and run the PC port normally with your own compatible retail Jaguar ROM.
2. In **F12 → Enhancements**, enable **Prototype Texture Restoration**.
3. The existing loader automatically searches for the `prototype_texture_restoration/` folder beside the executable, one directory above it, or in the current working directory. A CMake build in `build-gcc/` or `build-clang/` finds the pack at the repository root; the usual `LINUX/` executable layout finds it one directory above as well.
4. The enhancement should report that replacements have been applied. If it reports **0 panels**, check the folder placement and use `python3 tools/verify_prototype_texture_pack.py prototype_texture_restoration` from the repository root.

These are **128×128 RGB panels in the AVPTEX1 `.prgb` container**, not PNG files. The texture data remains derived from prototype-era game artwork; conversion does not create a new license for the artwork. See `prototype_texture_restoration/PROVENANCE.json` for the per-panel source IDs and checksums, plus [Licensing](LICENSE.md) and [Provenance](PROVENANCE.md). **No retail ROM is included**, and the texture enhancement does not replace the retail-ROM requirement.

## Map Maker

F12 → **Map Maker** opens the PC 2D/3D map editor. Implemented features include original-Jaguar texture browsing, floor/ceiling/wall painting, placement and selection of mapped objects, wall editing, camera navigation, multi-selection, undo/redo, save/load of custom map drafts, and Play Test. Object sprite previews are available for verified ROM asset mappings; unknown definitions remain markers. See [`new/map_maker/README.md`](new/map_maker/README.md) and the release-specific notes in [`docs/re33/`](docs/re33/).

**Not all possible edits are safe to playtest.** Relocating certain original scripted doors/level-transition data is guarded and may be intentionally disallowed in Play Test rather than silently corrupting gameplay. The editor's 3D in-world object billboards are not fully implemented for every object. The editor is **not** a guarantee of complete arbitrary map scripting support.

## Known limitations and unverified behavior

The most important distinction is **source implementation versus retail-ROM gameplay validation**:

1. **Not feature-complete:** Original Jaguar hardware parity—especially video composition, original GPU/DSP timing, 3D effects, FullSynth audio detail and some transitions—remains under evaluation.
2. **Multiplayer:** Four-player and 12-bot systems exist, but unusual respawn positions, level transfers, Fast Movement collision boundaries and AI edge cases still merit extended playtesting. Automated regression tests do not prove the absence of gameplay bugs.
3. **Animations:** Marine/Alien/Predator third-person animation behavior has been adjusted repeatedly to match source/reference footage; some exact visual cues still need human comparison.
4. **Bot AI:** The diagonal melee attack correction and other improvements are code-tested, not guaranteed correct for every live encounter or map layout.
5. **Map Maker:** Scripted door relocation, all object previews, arbitrary scripted behaviors and every editor interaction are not fully supported.
6. **Windows:** The current R71 state is primarily built and tested on Linux. Historical Windows builds do not replace current native Windows hardware verification.
7. **Audio/display hardware:** Monitor refresh, driver, controller, speaker configuration and fullscreen behavior can vary; some observations from one system may not generalize.
8. **Asset policy:** A compatible retail ROM is still required and is not included. The repository now also carries 51 converted prototype-art texture panels solely for the optional restoration enhancement; their copyright status and redistribution rights remain separate from the software source.

For a focused list of validated versus unverified functionality, see [`docs/FEATURE_STATUS.md`](docs/FEATURE_STATUS.md). Report issues with game mode, selected character, level, reproduction steps, enhancements enabled, and—when relevant—Global Positioning coordinates. Do **not** attach a copyrighted game ROM to a public issue.

## Repository layout

```text
src/           Reconstructed Jaguar gameplay and PC/Jaguar host backends
include/       Public and internal headers
new/           PC extensions: local multiplayer, bots, map maker, micro-opts
third_party/   Third-party source, with original licenses/notices retained
tests/         Source-level and regression tests
cmake/         Build/toolchain and linker scripts
tools/         Validation, build and Jaguar reference/research tools
docs/          Technical notes, feature status, current release history
prototype_texture_restoration/  51 optional AVPTEX1 .prgb texture panels + provenance
.github/       CI workflow source (not necessarily present in the GitHub default branch)
CMakeLists.txt Cross-platform CMake entry point
```

The repository excludes historical generated binaries, retail ROMs, runtime caches, personal saves, giant duplicate development logs and hundreds of cumulative documentation snapshots. The 51 documented prototype-derived texture replacements are an explicit exception to the previous no-derived-assets policy. Full historical release bundles are kept separately from the clean source tree. See [upload guide](UPLOAD_TO_GITHUB.md).

## Acknowledgements

**TheGreenThunder** — For helping with several aspects of the project, providing feedback, and playtesting.

## Copyright, licensing and provenance

**Please read [`LICENSE.md`](LICENSE.md), [`PROVENANCE.md`](PROVENANCE.md), [`CREDITS.md`](CREDITS.md), and [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) before copying, modifying or redistributing any part of this project.**

- *Alien vs Predator*, Atari Jaguar branding, original game code/design, names, artwork, sounds and related property belong to their respective rights holders. The reverse-engineered/reconstructed portions do **not** automatically become public domain or permissively licensed by being published on GitHub.
- **No project-wide MIT/GPL/Apache license is granted here.** The notice is intentionally conservative; independently authored components may require separate rights analysis or explicit licensing before reuse. Public availability of source code is not itself a license to copy or redistribute it.
- **Musashi** (in `third_party/musashi/`) retains its MIT terms and copyright notice. **SoftFloat** retains John R. Hauser's own terms and required attribution. Do not delete those notices.
- Historical proprietary source/tool binaries and retail ROMs must not be uploaded. The included 51 prototype-derived texture panels are a documented exception for this enhancement; **permission to redistribute them is not established by this repository**, and rights holders may object to their publication. The README's descriptions are *not* legal opinions about the status of the game's reverse-engineered code. The maintainer is responsible for reviewing publication rights and responding to rights-holder concerns.

The aim is preservation, study and faithful local play; **not** a claim to own the original intellectual property or to replace a legitimately obtained game cartridge.

## Contributions and maintenance

Bug reports and reproducible test cases are welcome. If submitting a change, keep verified working paths intact, identify the original Jaguar behavior being matched, avoid altering `src/game/` unless necessary and explicitly justified, and run the appropriate tests with GCC and Clang when possible. Do not add additional proprietary assets or credentials to commits and pull requests. Any new asset requires a separate rights and provenance review. This project does not promise rapid issue resolution or feature completion.

**Latest prepared source baseline: R71.** See [`docs/RE33_R71_RELEASE_NOTES.md`](docs/RE33_R71_RELEASE_NOTES.md) for the latest addition and [`docs/re33/`](docs/re33/) for recent component-level notes.
