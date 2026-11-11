#!/usr/bin/env python3
"""Offline ROM audit of the Jaguar title-logo call order (RE34 R77).

Reads only an owner-supplied canonical retail AVP.jag. Never writes/distributes
cartridge data. Checks actual 68000 code sites and order, NOT frame-accurate
GPU execution. The PC's separate 32-VBL background hold is provisional and
is deliberately NOT asserted as a literal in the retail program.
"""
import argparse
import hashlib
from pathlib import Path
import zlib

SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
CPU_SHA = 'c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53'
ORIGIN = 0x8000
# These are complete instruction sequences from the ROM's gzip-expanded
# Motorola 68000 program, not host-made approximations.
SITES = {
    0x01F118: '4eb90001fa3a',            # title script calls native reveal
    0x01FA3A: '4eb90001f84a4eb90001f082',  # clear front OP then draw setup
    0x01FA62: '4eb90001f9fc',            # prepare/copy background buffers
    0x01FA68: '4eb90001e530',            # Make_TLi (original OP lists)
    0x01FA6E: '21fc000000007fb4',        # original GPU phase initialized 0
    0x01FA76: '207c0002c5284eb90001c380',  # GPU source load
    0x01FAC6: '4eb90001e15a4eb90001e7b4',  # One_Tick, Update
    0x01FAD2: '207c00007f802010b0bc0000000266e4', # wait for GPU completion=2
    0x01FAE8: '4eb90001f554',            # first fixed logo buffer stamp
    0x01FAF4: '4eb90001f554',            # second buffer stamp
    0x01FB00: '4eb90001f554',            # third buffer stamp
    0x01FB06: '4eb90001e15a',            # next VBlank before hiding object
    0x01FB9A: '000000014eb90001e15a4eb90001e706',  # GPU phase=1, tick, Update2
    0x01FBB8: '4eb90001e15a4eb90001e706', # second tick and Update2
    0x01FBC4: '207c00007fb82010d0bc00000002',  # phase += 2
    0x01E69E: '4eb90001e15a',            # Make_TLi: initial One_Tick
    0x01E6AC: '4eb90001e7b4',            # Make_TLi: initial Update
    0x01E6B2: '4eb90001e15a',            # Make_TLi: second One_Tick
}


def audit(path: Path):
    rom = path.read_bytes()
    if len(rom) != 4194304 or hashlib.sha256(rom).hexdigest() != SHA:
        raise ValueError('Expected the canonical 4MiB Atari Jaguar AVP.jag')
    decomp = zlib.decompressobj(31)
    cpu = decomp.decompress(rom[0x3804:])
    if not decomp.eof or len(cpu) != 99072 or hashlib.sha256(cpu).hexdigest() != CPU_SHA:
        raise ValueError('Retail 68000 code stream does not match')
    for site, hx in SITES.items():
        expected = bytes.fromhex(hx)
        if cpu[site-ORIGIN:site-ORIGIN+len(expected)] != expected:
            raise ValueError(f'68000 title instruction mismatch at ${site:06X}')
    # Original ALIEN GPU's phase parameter is $23 = 35; the CPU uses a
    # GPU-completion flag rather than waiting for 32 VBlanks before animation.
    instruction = bytes.fromhex('20fc00000023')
    interval = cpu[0x01FA82-ORIGIN:0x01FAC6-ORIGIN]
    if instruction not in interval:
        raise ValueError('ALIEN GPU 35-step parameter missing')
    # PREDATOR's original phase loop increments by 2 to $23 (35).
    inc = bytes.fromhex('d0bc00000002')
    limit = bytes.fromhex('b0bc00000023')
    interval = cpu[0x01FBC4-ORIGIN:0x01FBE0-ORIGIN]
    if inc not in interval or limit not in interval:
        raise ValueError('PREDATOR 2-step phase loop missing')
    print('PASS canonical retail ROM and 99,072-byte original 68000 program')
    print(f'PASS {len(SITES)} original title and logo CPU instruction sites')
    print('PASS original ordering: background/OP setup, ALIEN GPU 35 phases, logo stamps, PREDATOR paired Update2 loop')
    print('LIMIT: no exact 32-VBlank background-only wait is established by these ROM instructions')
    print('LIMIT: file/resource-loading latency, GPU timing, OP publication, and captured display frames still require verification')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom', type=Path, required=True)
    audit(ap.parse_args().rom)
