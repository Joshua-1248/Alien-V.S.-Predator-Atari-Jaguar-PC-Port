# R185 — original CPU-frame source crosswalk and continuing hardware-fidelity ledger

## Authoritative identity

User-owned Jaguar retail ROM `AVP.jag`: 4,194,304 bytes, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Original 68000 `RDB.TEXT` gzip stream at `$3804` is 99,072 bytes, SHA-256 `c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53`. `RDB.DTA` at `$E840` is 60,248 bytes, SHA-256 `772745e9d429bce8efe884bc66461e754372e9cc22563dd50bd1268ab29bb8ba`. These prove original executable/data bytes, not every reconstructed C function's semantics.

## Original `MAZE.S::NextFrame` versus PC R184/R185

| Original CPU event (source order) | PC R184 | R185 |
| --- | --- | --- |
| Snapshot x/y/angle + AMP/GMP/creature pointers before update | Omitted | RESTORED |
| Snapshot view sine/cosine | Only live sine/cosine set | RESTORED |
| Alien bite moves GPU view snapshot, never physical x/y | Omitted GPU view offset | RESTORED |
| GPU screen dispatch | Separate PC renderer replacing Jaguar GPU | UNCHANGED host renderer |
| `UpdatePlayer()` | Called | PRESERVED |
| `UpdateAMPs()` | Called, but after Computer() on terminal tick | RESTORED ordering |
| Clamp negative energy and call `Process_EndGame()` | Called, but after Computer() on terminal tick | RESTORED ordering |
| `PreFrame()` | Called | PRESERVED |
| `SwapScreens()` | Missing | RESTORED between Pre/Post |
| `PostFrame()` | Called | PRESERVED |
| `DoPause()` | Called | PRESERVED before Computer() |
| `PlayAvP` tests `comp_panel` after NextFrame | Terminal entered immediately after UpdatePlayer | RESTORED |
| `PlayAvP` tests `new_level` after Computer | Correct host staging | PRESERVED |

## Wider review (not all items are completed)

| Subsystem | Status in this revision | Remaining non-proven hardware/runtime boundary |
| --- | --- | --- |
| Campaign restarts | R184 resets retained; GCC/Clang focused tests pass | Interactive save/restore gameplay sequence |
| Doors and normal level travel | R178 reset and R182 vent ordering retained | Full 15-level live door/airlock comparisons |
| AMPs and enemy persistence | Original-derived source untouched; baseline tests pass | Doorway/Queen/rare path geometry and heavy AMP tests |
| Alien cocoon and bite | Bite GPU snapshot restored | Hardware-specific bite visual effect and full Alien campaign |
| Gameplay frame/double buffer | Snapshot, one swap, computer-entry order corrected | Real TOM Object Processor buffer display parity |
| Terminals / keyboard | Existing terminal input/fade path retained | Real gameplay entry/exit video comparison |
| Death/ending | End-of-frame cleanup/exit sequencing retained | Original ending per-species rendered timing |
| High Scores | ROM glyph and scroll fixes retained | True GPU scrolling microcode, frame-by-frame parity |
| Title/character selection | Existing R170–R180 presentation fixes retained | GPU portrait zoom and credits still partially approximated |
| HUD/automap/pause | Existing tests and pause snapshot retained | Exact native OP layers, high-res dynamic scale |
| Music/SFX | R183 audio cleanup preserved | JERRY sample timing, wave table and DSP behavior |
| Voice/unloop allocation | Original CPU music source untouched | Stop/unloop effect tails under real DSP |
| Quicksaves/EEPROM | Focused tests pass | Cross-process/host endianness and actual restored video frame |
| Sprite/CRY draw pipeline | Previous parity checks retained | Native Jaguar pixel-centre/raster edge traces |
| Input and timing | Existing 10Hz CPU / 60Hz presentation | 68000 VSync and GPU timing/catch-up fidelity |
| Windows platform | No changes | No new Windows-native tests |
| Packaging and rights | Credits/licenses preserved; external ROM required | No private commercial asset distribution |

## Scope control

Do not confuse ROM stream matches with GPU/DSP execution certification. This pass changes no `src/game` files and adds no new game mechanics, artificial animation easing, diagnostic production patches, music boost, or game assets. Original code is the primary behavioral reference. Full CPU sequencing now conforms more closely to the recovered Jaguar code; original Jaguar TIM/OP peripheral effects still require separate validation.

---

# Alien vs Predator (1994 Atari Jaguar) — PC port R185

**Baseline:** R184, October 22, 2026. **Scope:** Full original Jaguar ROM-to-PC gameplay-frame and terminal-lifecycle sweep, retaining all prior systems.

## Three source-backed PC orchestration corrections

**1. Restore original NextFrame GPU-facing state snapshot.** The recovered retail `MAZE.S::NextFrame` publishes `gpu_xpos`, `gpu_ypos`, `gpu_angle`, the AMP and GPU resource pointers, and the view sine/cosine snapshot **before** running `UpdatePlayer`. It also applies `alien_bite` only to the GPU-facing x/y snapshot; it does not move the player. R184's PC simulation tick computed `sin_ang/cos_ang` but never set any of those GPU-facing fields, which could remain at stale initialization values. R185 restores the original assignments, with precisely the source's conditional Alien bite displacement. Modern host interpolation remains unchanged.

**2. Restore `SwapScreens()` between `PreFrame()` and `PostFrame()`.** Original `NextFrame` calls these three in this order, updating CPU-visible `screen_flip` and `build_screen` every gameplay tick. R184 omitted `SwapScreens()` completely; it only reset the double-buffer state on campaign entry (R184). R185 invokes the existing function in the original slot. The hosted OP `swap_screens` backend remains a no-op by design; we do not invent hardware swaps or modify the original assembly-derived code.

**3. Restore computer-entry timing relative to the full gameplay frame.** Original `PlayAvP` calls `NextFrame` (including `UpdateAMPs`, death/ending processing, overlays, buffer swap and `DoPause`) and *then* checks `comp_panel` and invokes `Computer()`. R184 opened the computer immediately after `UpdatePlayer`, then ran enemy/HUD/death updates after the computer session returned. R185 moves only the PC `comp_panel` bridge to after the frame completion and before level transitions, retaining the existing source-proven terminal FadeDown, input and computer logic. `launch_flag` completion also occurs after the existing full frame.

Only `src/platform/pc/pc_main.c` changes in production. **All 33 original `src/game` files are byte-identical to R184.** No ROM, extra game art or wave ROM is bundled; no testing instrumentation is inserted into production.

## Verification and limitations

- Original user-owned 4 MiB `AVP.jag` and both gzip sections `$3804`/`$E840`: **SHA-256 MATCH**.
- GCC and Clang strict Release builds: **PASS**.
- `tests/test_pc_gameplay_host_order.py` source-contract regression: **PASS on R185**, confirmed **FAIL on unchanged R184** because the GPU snapshot was missing. The regression locks the original CPU order, ensures exactly one `SwapScreens()`, preserves Alien view-only offset, and places the computer bridge after `DoPause()`.
- GCC and Clang focused campaign-restart, campaign-ending, music, levels/doors, MJP, frontend renderer/fade, savegame, terminal-roundtrip, and quickstate tests: **PASS** (20 total).
- GCC and Clang broad suites (excluding three historical very expensive cases): **PASS**.
- GCC and Clang original-ROM Xvfb startup smoke: **PASS through title/frontend resource loading**, including detection of all 15 retail mazes, 74 decoded JagPEG panel textures and FullSynth mixer initialization. Both unattended runs were intentionally terminated by `timeout` (signal 15); they **did not** enter a campaign or test the fixed terminal ordering.
- The three historical slow checks (`test_amp_passage_geometry`, `test_amp_projectile`, `test_sprite_column_cache`) and a live multi-campaign Jaguar comparison were not run. The snapshot and frame ordering match the recovered CPU C, not proven instruction-by-instruction or GPU-cycle parity.

## Next high-value ROM comparisons

Jaguar TOM Object Processor GPU list/fade timing, terminal/pause overlay interleaving, full JERRY FullSynth voice wave/percussion match, remaining source-derived end-game animation handoffs, and hard doorway AI path selection remain open. Preserve original AI/game behavior; do not claim full 1:1 emulation from these CPU fixes.
