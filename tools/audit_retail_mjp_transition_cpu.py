#!/usr/bin/env python3
"""Audit retail AVP MJP eight-VBlank 68000 CPU Object Processor X placement.

This deliberately does NOT implement or claim the Jaguar GPU's intermediate
pixel composition. It verifies the cartridge opcodes, signed phase branches,
position math, object build and eight calls to the original One_Tick.
No code/art is extracted into the release. Supply your own canonical AVP.jag.
"""
import argparse
import hashlib
from pathlib import Path
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
ORIGIN = 0x8000

# CPU instruction addresses, NOT raw cartridge offsets. Every pattern is a
# complete 68000 instruction or a contiguous sequence of complete instructions.
EXPECTED = {
    0x01DF2C: '7a00',                                # moveq #0,d5
    0x01DF2E: '23fc0004fc400004bee8',                # Ob_Addr <- $4fc40
    0x01DF38: '23f90004be180004beec',                # Ob_Data <- page buffer
    0x01DF42: '23f90004be240004bef4',                # Ob_Y <- High_Line
    0x01DF4C: '06b9000000100004bef4',                # Ob_Y += 16
    0x01DF56: '23fc000000900004bef8',                # height 144
    0x01DF60: '23fc000000c00004befc',                # width 192
    0x01DF6A: '23fc000000120004bef0',                # X=18
    0x01DF74: '0cb9000000010004bed4',                # transition mode == 1
    0x01DF7E: '67000016',                            # BEQ to $1df96
    0x01DF82: '0cb9ffffffff0004bed4',                # mode == -1
    0x01DF8C: '6700000e',                            # BEQ to $1df9c
    0x01DF90: '720a6000002c',                        # default D1=10, branch to $1dfc0
    0x01DF96: '220560000026',                        # mode 1 D1=D5
    0x01DF9C: '72189285',                            # mode -1 D1=24-D5
    0x01DFA0: 'b2b90004bed86d000008',              # if D1>=upper, D1=upper
    0x01DFB0: 'b2b90004bedc6e000008',              # if D1<=lower, D1=lower
    0x01DFC0: 'e589d3b90004bef0',                   # D1<<=2; Ob_X+=D1
    0x01DFC8: '4eb90001e240',                        # Make_New
    0x01DFCE: 'dabc00000001',                        # D5++
    0x01DFD4: '4eb90001e15a',                        # One_Tick
    0x01DFDA: 'babc000000086600ff4c',              # cmp #8,d5, BNE $1df2e
}


def cpu_position(mode: int, tick: int, upper: int, lower: int) -> int:
    """Unscaled Object Processor X constructed by the verified CPU path."""
    if not 0 <= tick < 8:
        raise ValueError('Expected exactly the eight retail D5 passes (0..7)')
    if mode == 1:
        d1 = tick
    elif mode == -1:
        d1 = 24 - tick
        # Retail signed CMP / BLT and CMP / BGT pairs, in that order.
        if d1 >= upper:
            d1 = upper
        if d1 <= lower:
            d1 = lower
    else:
        d1 = 10
    return 18 + (d1 << 2)


def audit(rom_path: Path):
    rom = rom_path.read_bytes()
    if len(rom) != 4194304 or hashlib.sha256(rom).hexdigest() != ROM_SHA:
        raise ValueError('Not canonical 4-MiB AVP.jag')
    decompress = zlib.decompressobj(31)
    code = decompress.decompress(rom[0x3804:])
    if not decompress.eof or len(code) != 99072:
        raise ValueError('Retail 68000 GZIP image differs')
    for address, hexdata in EXPECTED.items():
        expected = bytes.fromhex(hexdata)
        off = address - ORIGIN
        if code[off:off+len(expected)] != expected:
            raise ValueError(f'Unexpected 68000 opcode at ${address:06X}')
    # Ensure branches resolve to the audited mode dispatch and 8-VBlank loop.
    assert 0x01DF80 + int.from_bytes(code[0x1df80-ORIGIN:0x1df82-ORIGIN], 'big', signed=True) == 0x01DF96
    assert 0x01DF8E + int.from_bytes(code[0x1df8e-ORIGIN:0x1df90-ORIGIN], 'big', signed=True) == 0x01DF9C
    assert 0x01DFE2 + int.from_bytes(code[0x1dfe2-ORIGIN:0x1dfe4-ORIGIN], 'big', signed=True) == 0x01DF2E
    print(f'PASS {len(EXPECTED)} fixed retail 68000 instruction groups, 3 branch destinations')
    # Phase tables demonstrate only CPU-created object offsets. The GPU
    # image buffer's intermediate contents are NOT established by these tables.
    scenarios = [(0, 10, 0), (1, 10, 0), (-1, 10, 0), (-1, 24, 0), (-1, 24, 20)]
    for mode, upper, lower in scenarios:
        xs = [cpu_position(mode, i, upper, lower) for i in range(8)]
        print(f'  mode={mode:2d}, upper={upper:2d}, lower={lower:2d}: X={xs}')
    assert [cpu_position(1, i, 0, 0) for i in range(8)] == [18,22,26,30,34,38,42,46]
    assert [cpu_position(-1, i, 24, 0) for i in range(8)] == [114,110,106,102,98,94,90,86]
    assert [cpu_position(-1, i, 24, 20) for i in range(8)] == [114,110,106,102,98,98,98,98]
    assert [cpu_position(0, i, 0, 0) for i in range(8)] == [58] * 8
    print('PASS: CPU-owned eight-VBlank OP positions and exact native 192x144 geometry')
    print('LIMIT: Jaguar GPU pixel compositing, phrase publication and real display captures not verified')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    audit(parser.parse_args().rom)
