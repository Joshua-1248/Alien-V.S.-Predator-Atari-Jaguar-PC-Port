#!/usr/bin/env python3
"""Original retail AVP.jag MJP CROSS GPU payload and *post-8-VBlank* CPU handshake audit.

This is an offline source/ROM verifier, not production instrumentation or a GPU
emulator. It outputs no ROM executable bytes, proprietary pictures or sound.
Only pass a legally possessed unmodified 4-MiB Atari Jaguar AVP.jag.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
CPU_ORIGIN = 0x8000
RDB_ORIGIN = 0x20300

# Original assembly CPU addresses. All patterns begin and end on full 68000
# instruction boundaries (including their extension words).
CPU = {
    0x01C380: '4eb9000089404e75',      # local Load_GPU calls general GPU-loader, RTS
    0x01D838: '207c0002b8284eb90001c380', # A0=CROSS cartridge buffer, JSR Load_GPU
    0x01DFCE: 'dabc000000014eb90001e15abab c000000086600ff4c'.replace(' ', ''),
    0x01DFE4: '207c00007f80',          # GPU command/status shared block
    0x01DFEA: '20fc00000000',          # initial $7f80=0
    0x01DFF0: '20f90004be10',          # first scene-image buffer pointer
    0x01DFF6: '20f90004be14',          # second scene-image buffer pointer
    0x01DFFC: '20c6',                  # D6 original source buffer pointer
    0x01DFFE: '247c00007fa4',
    0x01E004: '2486',                  # D6 -> $7FA4
    0x01E006: '2206',                  # D1=D6 (original buffer state)
    0x01E008: '20c7',                  # D7 -> GPU command block
    0x01E00A: '20fc00000000',
    0x01E010: '20fc00000090',          # height 144
    0x01E016: '207c0004be20',
    0x01E01C: '20bc00000001',          # 1 -> $4BE20
    0x01E022: '207c00007fb4',
    0x01E028: '20bc00000001',          # start progress value 1
    0x01E02E: '23fc00f0300000f02110', # G_PC = F03000
    0x01E038: '23fc0000000100f02114', # G_CTRL = 1 (start GPU)
    0x01E042: '23fc0004fc400004bee8', # OP list slot 0
    0x01E04C: '23f90004bee40004beec', # new bitmap buffer pointer
    0x01E094: '207c00007fb4',         # repeated counter read
    0x01E09A: '2210',                 # D1 = $7FB4
    0x01E09C: 'd2bc00000008',         # D1 += 8
    0x01E0A2: '0cb9000000010004bed4', # transition mode +1
    0x01E0AC: '67000016',             # BEQ at 0x1e0ae -> 0x1e0c4
    0x01E0B0: '0cb9ffffffff0004bed4', # transition mode -1
    0x01E0BA: '6700000c',             # BEQ at 0x1e0bc -> 0x1e0c8
    0x01E0BE: '720a6000002c',         # D1 =10, branch to 0x1e0ee
    0x01E0C4: '60000028',             # mode +1 retains progress+8
    0x01E0C8: '741894812202',         # mode -1: D1=24-D1
    0x01E0CE: 'b2b90004bed86d000008', # upper-limit compare
    0x01E0DE: 'b2b90004bedc6e000008', # lower-limit compare
    0x01E0EE: 'e589',                 # four-pixel increments
    0x01E0F0: '23fc0004fc400004bee8', # build each new OP list
    0x01E13A: '4eb90001e240',         # Make_New
    0x01E140: '207c00007f80',         # GPU completion state pointer
    0x01E146: '2010',                 # D0 = $7f80
    0x01E148: 'b0bc00000002',         # complete iff GPU sets it to 2
    0x01E14E: '6600ff44',             # otherwise repeat from $1E094
    0x01E152: '44b90004bed4',         # NEG.L $4BED4 (reverse direction)
    0x01E158: '4e75',                 # returns ONLY after GPU completion
}

# Header addresses in original *uncompressed RDB.DTA*, not ROM file offsets.
# CPU MJP scene loader directly references CROSS. CROSSX and FADEIN are included
# to verify the neighboring recoverable GPU payload boundaries, not to imply
# they execute in the same scene-transition stage.
GPU = [
    ('CROSS', 0x2B828, 1488, '0300cb1bf633acbd620f8185b144737fabb596ffce7cb52708d2bf44afb321e5'),
    ('CROSSX', 0x2BE00, 1824, 'd42901b46b285f63ec9363c5f8c627e71b43ea98cd6078b587f278f4b11c8eb0'),
    ('FADEIN', 0x2C528, 1288, '3dd5d1d8f74b67bdd09f057c6f70b2247611c4c863c8ac98a60c7022a091c080'),
]


# Verified static MJP setup writes in their original executed CPU routines.
# A write at one location is not evidence that every subsequent call sees that
# value: CPU branches and NEG.L at exit change runtime state.
SETUP_WRITES = {
    # destination $4BED4 = direction, $4BED8 upper, $4BEDC lower
    0x01D89A: (0x04BEDC, 0),
    0x01D8A4: (0x04BED4, 0),
    0x01D8B4: (0x04BED4, -1),
    0x01D904: (0x04BED8, 10),
    0x01D930: (0x04BED8, 99),
    0x01D9E4: (0x04BEDC, 10),
    0x01DA0C: (0x04BEDC, 10),
    0x01DA16: (0x04BED8, 10),
    0x01DA78: (0x04BED4, 0),
    0x01DB40: (0x04BED4, 0),
    0x01DB82: (0x04BED4, 0),
    0x01DBA6: (0x04BEDC, 10),
    0x01DBB0: (0x04BED8, 10),
}


def extract(rom: bytes, start: int, size: int) -> bytes:
    obj = zlib.decompressobj(31)
    value = obj.decompress(rom[start:])
    if not obj.eof or len(value) != size:
        raise ValueError(f'Incorrect retail gzip stream ${start:X}: {len(value)} bytes')
    return value


def check_branches(code: bytes) -> None:
    # Motorola 68000 16-bit branch displacement is relative to the extension
    # word's address; do not use the end-of-instruction as the base.
    for extension, target in (
        (0x01DFE2, 0x01DF2E),
        (0x01E0AE, 0x01E0C4),
        (0x01E0BC, 0x01E0C8),
        (0x01E0C2, 0x01E0EE),
        (0x01E0C6, 0x01E0EE),
        (0x01E150, 0x01E094),
    ):
        offset = int.from_bytes(code[extension-CPU_ORIGIN:extension-CPU_ORIGIN+2], 'big', signed=True)
        if extension+offset != target:
            raise ValueError(f'Bad original branch at ${extension:06X}: ${extension+offset:06X} != ${target:06X}')


def run(path: Path) -> None:
    rom = path.read_bytes()
    if len(rom) != 4194304 or hashlib.sha256(rom).hexdigest() != ROM_SHA:
        raise ValueError('Not the verified unmodified 4 MiB Jaguar AVP.jag')
    code = extract(rom, 0x3804, 99072)
    data = extract(rom, 0xE840, 60248)
    for address, expected_hex in CPU.items():
        expected = bytes.fromhex(expected_hex)
        observed = code[address-CPU_ORIGIN:address-CPU_ORIGIN+len(expected)]
        if observed != expected:
            raise ValueError(f'Original 68000 instruction mismatch at ${address:06X}')
    check_branches(code)
    for cpu_addr, (dest, value) in SETUP_WRITES.items():
        # MOVE.L #signed_32,(absolute_long), exact instruction and destination
        expected = struct.pack('>HII', 0x23FC, value & 0xFFFFFFFF, dest)
        if code[cpu_addr-CPU_ORIGIN:cpu_addr-CPU_ORIGIN+10] != expected:
            raise ValueError(f'Original MJP setup write mismatch at ${cpu_addr:06X}')
    for name, address, n, expected_sha in GPU:
        i = address-RDB_ORIGIN
        header = data[i:i+8]
        if len(header) != 8:
            raise ValueError(f'{name} truncated header')
        load_target, nbytes = struct.unpack('>II', header)
        if load_target != 0xF03000 or nbytes != n:
            raise ValueError(f'{name} unexpected GPU-load target or count')
        image = data[i+8:i+8+nbytes]
        if len(image) != n or hashlib.sha256(image).hexdigest() != expected_sha:
            raise ValueError(f'{name} GPU payload hash mismatch')
        print(f'PASS {name:6} RGB16 GPU image: {nbytes} bytes at GPU $F03000; SHA256={expected_sha}')
        if name == 'CROSS':
            for mask, gpu_address in (
                (0x7BDF7BDF,0xF03072),
                (0x39CF39CF,0xF03078),
                (0x18C718C7,0xF0307E),
                (0x08430843,0xF03084),
            ):
                j = gpu_address-0xF03000
                if image[j:j+4] != struct.pack('>I',mask):
                    raise ValueError(f'CROSS RGB16 fractional-channel mask at ${gpu_address:06X} differs')
    print(f'PASS {len(CPU)} original 68000 opcode blocks, six branch targets and {len(SETUP_WRITES)} scene parameter writes')
    print('PASS 8-VBlank phase -> GPU command block -> GPU start -> OP publication/poll -> direction negate -> RTS')
    print('BOUNDARY: this verifies original CPU/GPU buffer protocol and payload identity; no cycle-level GPU playback or screen parity claim.')

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', required=True, type=Path)
    run(p.parse_args().rom)
