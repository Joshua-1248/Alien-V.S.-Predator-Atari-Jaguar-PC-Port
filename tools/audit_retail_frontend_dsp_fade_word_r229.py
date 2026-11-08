#!/usr/bin/env python3
"""R229 offline ROM-anchored audit of original F1BB40 accesses and fades.

No diagnostic game code, ROM extraction, or inferred DSP state. Requires locally
owned AVP.jag; the original executable bytes are compared, never distributed.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
PROGRAM_SHA = 'c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53'
BASE = 0x8000
DSP_WORD = 0xF1BB40
STEP = 0xFA

# Exact instruction starts, independently classified (not offsets of the
# address literal). The same address appears in other RAM domains too.
OPERATIONS = {
    'set_full': (0x86F0, 0x8872, 0x985E, 0xCECE, 0xCF20, 0xF1C8),
    'set_zero': (0xCE8A, 0x1C1D6),
    'clear': (0xCEEA,),
    'read': (0x9ACA, 0x9AF6),
    'write_d0': (0x9AEC, 0x9B1E),
}
BYTES = {
    'set_full': bytes.fromhex('23fc00007fff00f1bb40'),
    'set_zero': bytes.fromhex('23fc0000000000f1bb40'),
    'clear': bytes.fromhex('42b900f1bb40'),
    'read': bytes.fromhex('203900f1bb40'),
    'write_d0': bytes.fromhex('23c000f1bb40'),
}
CALLS_DOWN = (0x1CE16, 0x1EF4E, 0x1F88A)
CALLS_UP = (0x1C0B0, 0x1C1F4)


def checked_program(path: Path) -> bytes:
    rom = path.read_bytes()
    assert len(rom) == 4194304 and hashlib.sha256(rom).hexdigest() == ROM_SHA, 'unexpected AVP.jag'
    z = zlib.decompressobj(31)
    code = z.decompress(rom[0x3804:])
    assert z.eof and len(code) == 99072
    assert hashlib.sha256(code).hexdigest() == PROGRAM_SHA
    return code


def raw(code: bytes, pc: int, n: int) -> bytes:
    assert BASE <= pc and pc+n <= BASE+len(code)
    return code[pc-BASE:pc-BASE+n]


def exact(code: bytes, pc: int, data: bytes) -> None:
    got = raw(code, pc, len(data))
    assert got == data, f'${pc:06X}: {got.hex()} != {data.hex()}'


def all_literal_uses(code: bytes) -> list[int]:
    needle = struct.pack('>I', DSP_WORD)
    return [BASE+i for i in range(0, len(code)-3, 2) if code[i:i+4] == needle]


def fade_down(initial: int, step: int) -> tuple[int,int]:
    # 68000 SUB.L D0,(count) with BPL on signed 32-bit result.
    v, frames = initial, 0
    while True:
        v -= step
        frames += 1
        if v < 0:
            return 0, frames
        assert frames < 50000


def fade_up(initial: int, step: int) -> tuple[int,int]:
    # 68000 ADD.L and TST.W, BPL on the low *word*, not whole long.
    v,frames = initial,0
    while True:
        v += step
        frames += 1
        if (v & 0x8000):
            return 0x7fff, frames
        assert frames < 50000


def audit(code: bytes) -> None:
    covered=[]
    for kind,locations in OPERATIONS.items():
        for pc in locations:
            b=BYTES[kind]
            exact(code,pc,b)
            # Every site carries an embedded absolute long-address operand.
            literal=pc+len(b)-4
            covered.append(literal)
    observed=all_literal_uses(code)
    assert sorted(covered)==observed, (len(covered),len(observed),set(observed)-set(covered))
    assert len(observed)==13

    # Fade-control call sites must use MOVE.L #$FA,D0 immediately before
    # calling the original installer; these are all direct calls in ROM.
    for target, sites in ((0x9A5C,CALLS_DOWN),(0x9A80,CALLS_UP)):
        instruction = bytes.fromhex('203c000000fa4eb9') + struct.pack('>I',target)
        for pc in sites:
            exact(code,pc-6,instruction)
        direct = bytes.fromhex('4eb9')+struct.pack('>I',target)
        found=[BASE+i for i in range(0,len(code)-5,2) if raw(code,BASE+i,6)==direct]
        assert found==list(sites), (hex(target),found)

    # The actual return-to-previous-VBlank-handler / audio-state operations
    # were independently audited in R228; pin the key arithmetic/branch
    # instructions here as proof of endpoint behavior and word-width.
    exact(code,0x9AD0,bytes.fromhex('90b90002ef44')) # SUB.L countdown,D0
    exact(code,0x9AD6,bytes.fromhex('6a000014'))     # BPL, no clamp
    exact(code,0x9ADA,bytes.fromhex('4280'))         # CLR.L D0
    exact(code,0x9AFC,bytes.fromhex('d0b90002ef44')) # ADD.L countdown,D0
    exact(code,0x9B02,bytes.fromhex('4a40'))         # TST.W D0
    exact(code,0x9B04,bytes.fromhex('6a000018'))     # BPL, no clamp
    exact(code,0x9B08,bytes.fromhex('203c00007fff')) # MOVE.L #7fff,D0
    exact(code,0x9AE2,bytes.fromhex('23f90002ef400002ee80'))
    exact(code,0x9B14,bytes.fromhex('23f90002ef400002ee80'))

    # The visual fade's CPU memory-word zeroing before timer B in mode 3.
    exact(code,0x1C1D6,BYTES['set_zero'])
    exact(code,0x1C1EE,bytes.fromhex('203c000000fa'))
    exact(code,0x1C1F4,bytes.fromhex('4eb900009a80'))

    d=fade_down(0x7fff,STEP)
    u=fade_up(0,STEP)
    assert d==(0,132) and u==(0x7fff,132), (d,u)
    assert 0x7fff-131*STEP==17 and 131*STEP==32750
    assert 0x7fff-132*STEP==-233 and 132*STEP==33000
    print('PASS authenticated retail 4 MiB ROM and 99,072-byte 68000 executable')
    print('PASS 13/13 absolute references to $F1BB40 classified (6 full, 2 zero, 1 CLR, 2 read, 2 D0 write)')
    print('PASS all five direct fade installer callsites use MOVE.L #$FA,D0')
    print('PASS original SUB.L/BPL fade-down and ADD.L/TST.W/BPL fade-up endpoints')
    print('PASS 132 timer-callback steps from $7FFF to 0 and 0 to $7FFF at step $FA')
    print('LIMIT: callback steps are not verified absolute VBlank durations; producer/consumer DSP routines, real audio and pixel timing remain unproven')

if __name__ == '__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',required=True,type=Path)
    audit(checked_program(ap.parse_args().rom))
