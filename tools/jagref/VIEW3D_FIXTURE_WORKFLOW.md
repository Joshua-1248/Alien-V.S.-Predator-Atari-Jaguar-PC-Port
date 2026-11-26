# RE24 Sweep 28 — reusable Jaguar 3D-view parity fixtures

This is a **GPU-entry capture/replay system**, not a whole-game savestate or
final TV-frame equivalence test. It replays the original JRISC 3D view and the
verified native C stages 0–2 from the same private state and compares all
Blitter commands, 2 MiB DRAM, and the architectural fields checked by
`view3d_port_check.c`. It excludes the GPU's r28–r31 scratch regs and does not
verify complete Object Processor scanout, sound or physical-console timing.

## Record once (user's original cartridge required)

```bash
cmake -S . -B build-gcc -DCMAKE_C_COMPILER=gcc -DAVP_BUILD_JAG_REFERENCE=ON
cmake --build build-gcc --parallel 4
bash tools/jagref/view3d_fixture_suite.sh capture '/private/Alien vs. Predator.jag' '/private/avp_view_fixtures' build-gcc
```

The ten reference poses boot through Alien, Predator and Marine. The record
step can be CPU-intensive; each snapshot is about 2.1 MiB, with full writable
DRAM, GPU-local RAM and explicit GPU/Blitter register state. All snapshots,
checksums and capture logs are stored under the private output directory.
Snapshots must never be checked into git or included in a distributable ZIP.

## Replay repeatedly without booting the cartridge

```bash
bash tools/jagref/view3d_fixture_suite.sh replay '/private/Alien vs. Predator.jag' '/private/avp_view_fixtures' build-gcc 50
CC=clang bash tools/jagref/view3d_fixture_suite.sh replay '/private/Alien vs. Predator.jag' '/private/avp_view_fixtures' build-clang 50
```

Each snapshot is checked against the manifest SHA-256 and an internal payload
checksum, format version and cartridge fingerprint before use. A mismatched or
corrupted fixture is refused. Replays still require a user-supplied verified
retail ROM because some Jaguar GPU bus reads access cartridge address space.
The `--replay` mode runs an original JRISC view followed by native C from the
same entry state and optionally fuzzes maze/camera/objects; no 68000 boot is
repeated. This is a fast regression loop for the original 3D GPU renderer.

To compare end-to-end final RGB images or complete game frames, the Object
Processor, presentation, audio and live scene-asset builder must still be
integrated and validated. Do not claim those are implemented by this tool.

## Raw single-view CLI

The source's `view3d_port_check.c` also accepts:

```
view3d_port_check ROM --capture PRIVATE.avpview FIELD INPUT [FUZZ_SEEDS]
view3d_port_check ROM --replay  PRIVATE.avpview [FUZZ_SEEDS]
```

The fixture is intentionally pointer-free and little-endian; it captures
only the fields used by the currently tested GPU path. It does **not**
serialize the Musashi CPU, TOM OP state, DSP or complete bus-timing state.

## Combined build + regression + view-replay gate

After capturing your private ten-view fixture set, this one command builds both
compilers, runs their **complete regression suites sequentially**, then replays
all captured views with the requested additional randomized views:

```bash
bash tools/jagref/view3d_verify_gate.sh \
  '/private/Alien vs. Predator.jag' '/private/avp_view_fixtures' \
  '/private/verification_report' 20
```

The gate emits `checks.tsv` and logs, and exits nonzero if any check fails.
Keep the output separate from the private fixtures. Replay logs are stored
beside private fixture files by the low-level test script; if you distribute
only the report, review the logs for unexpected private paths first. This gate
only verifies the current 3D-view pipeline and existing unit regressions; it
is **not** a physical-console or complete audio/video fidelity certification.
