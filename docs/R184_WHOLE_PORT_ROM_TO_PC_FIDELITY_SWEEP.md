# R184 — Full-port original Jaguar ROM versus PC lifecycle & hardware fidelity ledger

## Basis of audit

Verified user-owned retail ROM `AVP.jag`, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Stream at offset `$3804` inflates to 99,072 bytes (RDB.TEXT SHA-256 `c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53`); stream at `$E840` inflates to 60,248 bytes (RDB.DTA SHA-256 `772745e9d429bce8efe884bc66461e754372e9cc22563dd50bd1268ab29bb8ba`). These confirm an authoritative CPU/data byte baseline, **not** correctness of every higher-level C reconstruction. GPU/DSP and OP behavior is a separate hardware domain.

## Original first-campaign call sequence versus PC

| Phase | Original reconstructed 68000 sequence | Hosted R183 behavior | R184 disposition |
| --- | --- | --- | --- |
| `InitGame`: display buffers | `InitDblBufs` before `InitLevels` | Missing on fresh campaign | RESTORED for title and developer new game |
| `InitGame`: initial level | `InitLevels` | Called for title entry; safe developer entry differs intentionally | UNCHANGED |
| `InitGame`: global sprites | `InitSprites` | Missing | RESTORED |
| `InitGame`: player/AMP/terminal | `InitPlayer`, AMP GPU-list addresses, `InitAMPs`, `InitComp` | Functions already called, hardware pointers retained as backend seams | NO speculative hardware-pointer change |
| `LoadLevel`: scaling / resource transfer | `ResetScale`, `TransMaze`/`GetPanels` | PC decoded-mesh and resource bind followed by ResetScale | RETAIN current resource architecture |
| `LoadLevel`: sprite expansion | `ResetSprites` | Missing at first level only; subsequent levels already call it | RESTORED at first level |
| `LoadLevel`: player placement | `SetStart`, `restore_level`, clear transient cocoon position, spawn cleanup, `ResetPlayer` | Existing steps present | UNCHANGED, verified order |
| `PostFirst`: first-person overlay | `InitScrOverlays` after LoadLevel, then screen/list/fade | `Init1stOvers` and `InitHUDPal` happened before LoadLevel finished | MOVED after ResetPlayer, before clearing `savegame`; no duplicate `InitHUD` |
| `PostFirst`: display/fade | `ScreenOff`, `SetMazeList`, `FadeUp` | Already present | PRESERVED |
| `load_complete`: level sound / cocoon state | release `levfx_ID`, alarm (when appropriate), clear `use_cocoon` | Previously established for travel; `InitDoors` already resets initial handle | No redundant reset added |
| `ResetMaze`: facehugger | `hug_init` | Missing at initial hosted campaign entry | RESTORED |
| `ResetMaze`: automap and GPU | `ResetMap`, `ResetMGPU` | Existing host paths | PRESERVED |
| `ResetMaze`: bite view | `alien_bite=0` | Missing on fresh campaign | RESTORED |
| `ResetMaze`: ambient | `Ambient()` | Existing single voice allocation | PRESERVED without duplicate sound |

## Broader subsystems reviewed / preservation decisions

| Area | Source surface | Current disposition | Proof gap / next useful evidence |
| --- | --- | --- | --- |
| Doors and level access | `doors.c`, `levels.c`, PC maze-cache reset | R178 ROM-backed fresh-campaign door restoration retained | Full interactive reopen/revisit across 15 levels, exact door-panel OP raster |
| Enemy corpses / persistence | `amp.c::ram_save`, `rebuild_level`, `KillBastards` | R171 dead-AMP retention retained | Verify every template/entry-square exception with Jaguar play capture |
| Cocoon / Alien revival | `hud.c`, `amp.c`, `mazescrn.c` | C regressions retained; no new rules | Complete regular Alien playthrough and original animations |
| Enemy AI navigation | `amp.c`, `collision.c` | Existing width-aware collision and pursuit preserved | Rare door/archway and Queen boundary hardware comparison |
| Projectile/collision | `collision.c`, `amp.c` | Unchanged | Very expensive geometry/projectile regressions excluded in R184 |
| Terminals / computers | `computer.c`, `font.c`, PC runtime bridge | Input/typewriter/overlay regressions passed | Live exit/enter fade and original GPU text composition |
| HUD and automap | `hud.c`, `hud_message.c`, GPU HUD and tracker | Existing tests passed | Jaguar native OP final-surface compare at every resolution |
| Pause presentation | `mazescrn.c::DoPause`, video frontend | R178 current-frame host capture retained | Real high-resolution overlay/playtest |
| Quickstate + EEPROM | `savegame.c`, `pc_quickstate.c`, `eeprom.c` | Cross-level save tests passed; new campaign retains saved-data consumption | Live cross-process save and repeated campaign cycle |
| Frontend title / text | `mjp.c`, `frontend_renderer.c` | ROM-derived fonts and title assets retained | Exact title state GPU/OP composition still partly host approximated |
| Selected portrait and loading | `mjp.c::Do_Selec`, PC presentation | R172–R174 timings and gates retained | Remaining crossfade/OP pixel and loading-delay implementation from GPU program |
| Game Over/High Scores | `mjp.c::Do_Lose`, `Do_Fame`, GPU frontend | R175–R180 corrections retained | Original fade/scroll microcode; exact score glyph appearance at all phases |
| Credits/attract | `mjp.c` and host renderer | Functionality retained; credit pages are host-authored, not original reconstruction | Original credit OP/timing and text resources |
| Duct/terminal fade | `maze.c::PlayAvP`, `pc_main.c` | R182 source-order correction retained; terminal pre-entry wait retained | True interrupt-driven overlap and TOM fade ramp versus retail hardware |
| Gameplay timing | `maze.c`, `pc_main.c` | Original 10 Hz game state; host 60 Hz redraw/presentation | Native VBlank/VC synchronization, especially frame skipping |
| Jaguar TOM GPU | `src/jaguar/avp_gpu/` | CPU/GPU host replacements; major pixel regressions passed | Original RISC program instruction/OP output exactness not established |
| Jaguar JERRY audio | `music.c`, `fullsynth_host.c`, PC audio | Existing priority, loop, ambient fixes preserved | Actual FullSynth wavetable/program/voice matching, no guessed gain |
| Music exit lifecycle | `maze.c`, `pc_main.c` | R183 KillAlar/KillAll/KillAmbi preserved | DSP stop/unloop latency on genuine hardware |
| Frontend controls | `pc_video_x11.c`, `pc_runtime.c` | Rebindable Jaguar-like menu controls retained | Hardware controller scan semantics and UI comparison |
| Host Windows output | `src/platform/pc` Windows variants | Original sources preserved, native Linux compilers pass | Windows build/run not verified this pass |
| Attribution/packaging | LICENSE, CREDITS, third-party notices | Preserved; no commercial ROM distributed | Respect original rights and third-party licenses |

## Deliberately not changed

No arbitrary audio gain or waveform approximation, no speculative AI or new collision model, no forced credits animation, no recreated copyrighted assets, no gameplay diagnostic instrumentation, and no alteration to the working ROM resource layout. `InitHUD` already runs in `InitPlayer`; do not add an unverified second call merely to mimic the name of `StdHUD`, which could disturb restored cocoon state. GPU callback identities that intentionally map to hosted renderer seams are not claimed to be exact Jaguar GPU execution.

## Test extent

Both GCC and Clang strict native Linux builds PASS. Broad source-level suites ran via a combined runner and smaller follow-up runners; all completed groups PASS. Three very slow tests were excluded (`test_amp_passage_geometry`, `test_amp_projectile`, `test_sprite_column_cache`). GCC and Clang ASan/UBSan of the new initial-state reset test PASS. The user's ROM reached the retail frontend under Xvfb; this does not exercise all modified campaign-entry paths or prove runtime visual parity. See `R184_VERIFICATION.md` for precise boundaries.
