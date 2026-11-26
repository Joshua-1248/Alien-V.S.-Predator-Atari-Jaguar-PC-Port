# RE24 S29 — private JagPEG loading-transaction capture/replay

The original retail AVP cartridge feeds two **distinct GPU jobs** during level loading:
the level JPEG decoder at GPU `$F03000` and the RGB-to-CRY converter at
`$F03A00`. The independent `jagpeg_port_check.c` compares each original JRISC
job to the hand-ported native C `avp_jagpeg_gpu_decode/convert`, including
Blitter commands, **all 2 MiB DRAM**, **GPU local RAM** and decoder mailbox.
This tool saves the job **entry state**, not a complete Jaguar savestate.

To avoid rebooting the cartridge for repeated decoder changes, Sweep 29 adds
`--capture` and `--replay`. The private snapshot uses the same pointer-free,
versioned, little-endian GPU/Blitter state serializer as Sweep 28, but a
**different magic**, exact entry-PC check, original ROM fingerprint, payload
checksum, exact length, and owner-only file permissions. It never copies
host pointers, CPU states, OP frames or DSP states. The decoder and converter
are different modes and cannot load each other's files.

## Capture all 126 original jobs ONCE

```bash
cmake -S . -B build-gcc -G Ninja -DCMAKE_C_COMPILER=gcc -DAVP_BUILD_JAG_REFERENCE=ON
cmake --build build-gcc --parallel 4
bash tools/jagref/jagpeg_fixture_suite.sh capture /private/Alien\ vs.\ Predator.jag /private/avp_jpeg_fixtures build-gcc
```

The script records **13 Alien, 10 Predator, 40 Marine** decoder jobs and the
same number of converter jobs. Each file is ~2.1 MiB, so budget at least
**270 MiB** in the private directory. It also writes manifest SHA-256 values
and private capture logs. A preexisting manifest or snapshot is not silently
overwritten. The capture must finish successfully before treating the set as
complete. The script itself was source/CLI-validated on Sweep 29; **all 126
capture jobs have not been independently replayed in this sweep.**

## Repeat parity without booting the game

```bash
bash tools/jagref/jagpeg_fixture_suite.sh replay /private/Alien\ vs.\ Predator.jag /private/avp_jpeg_fixtures build-gcc
CC=clang bash tools/jagref/jagpeg_fixture_suite.sh replay /private/Alien\ vs.\ Predator.jag /private/avp_jpeg_fixtures build-clang
```

Replay verifies the script's SHA manifest and internal CRC-style FNV checksum,
then compares the original JRISC GPU and native C from precisely the same
saved decoder/converter entry. **A verified copy of your own retail cartridge
is still required:** the GPU could read ROM address space outside captured
writable DRAM. The full-retail cartridge SHA-256 is checked by the script.

Single-job diagnostics (useful before capturing the full 126):

```bash
# PRIVATE_DIR must already exist with mode 700.
./jagpeg_port_check AVP.jag --capture PRIVATE_DIR 1640 1 1760 decode alien
./jagpeg_port_check AVP.jag --replay  PRIVATE_DIR 1640 1 1760 decode alien
./jagpeg_port_check AVP.jag --capture PRIVATE_DIR 1640 1 1760 convert alien
./jagpeg_port_check AVP.jag --replay  PRIVATE_DIR 1640 1 1760 convert alien
```

**Important:** These are GPU-level original-ROM comparisons, **not a completed
FILES dispatcher, panel loader, native game scene renderer, or physical-frame
parity test**. No screenshot, music/audio or console timing is validated here.
Never distribute `.avpjpeg` or `.avpview` files, decoded resource data,
`AVP.jag`, or original decoder-generated GPU C. Keep them out of public ZIPs.
