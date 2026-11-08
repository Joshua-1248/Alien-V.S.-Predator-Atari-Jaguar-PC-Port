# RE26 / Sweep 66 — actual retail 2P video restored and independently verified

## Retail-ROM failure, original source cause, and correction

Sweep 65's opt-in `--mp-walktest-2` path successfully entered the original level
and advanced two original player states but captured an all-black X11 game frame.
The original `src/game/maze.c` retail campaign entry leaves the source's
`fade_level=0x80`, `fade_lim=0`, `fade_rate=-2` for fade-in. The existing
single-player `pc_main.c` calls `MazeList()` at original Jaguar 60 Hz display
cadence. `src/game/mazescrn.c::MazeList()` alone reduces `fade_level` on each
video tick. The original GPU renderer at `software_renderer.c` deliberately
blacks out **every pixel including HUD** while `fade_level>=0x80`.

The S65 multiplayer loop never called `MazeList()`; the fade stayed at the
completely-black endpoint forever. Sweep 66 restores the *existing original
function*, at a separate ~60 Hz display cadence in the **single-world,
same-species, two-player experiment only**. It uses the original source's
increment/no-backlog scheduling pattern from `pc_main.c`, not an invented fade.
Normal single-player code is not modified. This is **not** a general
multi-world display clock; additional world-owned video state isolation is
required before extending it to independent concurrent levels.

## Actual pixel verification using the user's own unmodified retail AVP.jag

- ROM SHA-256: `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`.
- The port loads authored maze/AMP records, 74 JagPEG panel textures, original
  Marine HUD and original renderer; **no diagnostic geometry substituted**.
- Linux Xvfb: **one OS game process, one game window**, two independent 320x228
  original-renderer source frames composed side by side. Captures sampled
  after the original 60 Hz fade finished contain ~90,000 non-black root pixels.
- Live P2-only key input: P1 position stays `0x00018000` while P2 moves through
  original source `MovePlayer()` and valid retail cells. A before/after root
  screenshot pixel diff showed P1 viewport **0 differing pixels** and P2
  viewport **26,904 differing pixels** in a 2-second key hold, verifying the
  displayed P2 view is not copied from P1. A second fresh temporary-binary
  run of the repeatable test showed P1=0, P2=36,184.
- `new/multiplayer/tests/re26_retail_two_view_smoke.py` automates original-ROM
  boot, X11 real pixel capture, split-screen non-black checks, focus, P2-only
  keyboard input, and independent camera pixel comparison. It uses a **temporary
  copy of the executable and isolated settings.cfg**. Requires `Xvfb`, `scrot`,
  `Pillow`, system `libX11` and `libXtst` (optional test dependencies, never
  required by the actual game). Pass `--rom` explicitly; do not distribute ROM.
- All 16 existing Linux multimedia regression executables pass under Xvfb,
  including the one-window native pixel readback check. Strict GCC/Clang
  original `avp_pc` build and multiplayer-disabled build pass. Focused
  ASan/UBSan player-step + native context tests pass.

## Reproduce

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build --target avp_pc
./build/avp_pc --rom /path/to/your/AVP.jag --diagnostic --mp-walktest-2
python3 new/multiplayer/tests/re26_retail_two_view_smoke.py \
    --exe build/avp_pc --rom /path/to/your/AVP.jag
```

`--diagnostic` here skips the original title menu, **not** original ROM level
assets. For normal menu flow omit it and choose the campaign first. Player 2
keys can be configured in F12 Local Players; the test uses P2 `i` to move
forward. This verifies one native Linux test environment, NOT Windows behavior.

## Honest limits

Actual original HUD and camera pixels are shown, but this walktest does **not**
include the full NextFrame/PreFrame/PostFrame/UpdtHUD gameplay pipeline, and
several weapon status elements are not advanced. Enemy targeting, attacks,
inter-player combat/collision, deaths/respawns, terminals, level transitions,
co-op saves, mixed species, independent levels/video clocks, physical multi-mouse
Windows Raw Input, Windows runtime and campaign completion remain unfinished.
**Start Co-op is not enabled.** Do not call this full multiplayer or a retail
visual-parity guarantee. The original Jaguar single-player game remains the
fidelity reference; these additional co-op behaviors have no original rules.
