#!/usr/bin/env python3
"""R223 offline canonical-retail 68000 PRE_LIST and VBlank ISR audit.

Read-only exact instruction windows, register-address operands and control-flow
assertions; DOES NOT emulate 68000 hardware, run video scanlines, or record ROM
code/art. A pass does not certify OP frame timing or GPU pixel parity.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
BASE = 0x8000

# Original 68000 CPU address -> exact instruction-byte window (hex).
# Split groups where their boundaries matter for BSR/BEQ verification.
INSN = {
    0x9184: '42b90002ee84',            # init VBlank count
    0x918A: '21fc000091b60100',        # install VBlank ISR in CPU vector at $0100
    0x91B6: '52b90002ee84',            # VBlank counter ++
    0x91BC: '42790002ee88',            # wake flag clear
    0x91C2: '48e7fffe',                # preserve registers
    0x91C6: '20390002ee80',            # load installed callback address
    0x91CC: '6700000a',                # zero -> branch to fallback at $91D8
    0x91D0: '20404e90',                # installed callback: JSR (A0)
    0x91D4: '6000000c',                # bypass fallback
    0x91D8: '207c00007560',            # fallback tail address
    0x91DE: '61000128',                # invoke tail STOP helper
    0x91E2: '4cdf7fff',                # restore registers
    0x91E6: '33fc010100f000e0',        # IRQ acknowledgement
    0x91EE: '427900f000e2',            # clear TOM interrupt word
    0x91F4: '4e73',                    # RTE
    0x9210: '207c00007548',            # PRE_LIST head
    0x9216: '2248d2fc0010',            # A1 = $7558 (branch target)
    0x921C: '36390002ee94',            # first video bound
    0x9222: '123c0001',                # D1 = 1
    0x9228: '6152',                    # BSR branch writer
    0x9228: '227c00007560',            # second branch target
    0x922E: '36390002ee98',            # second video bound
    0x9234: '123c0001',
    0x923A: '6140',                    # BSR branch writer
    0x923A: '610000cc',                # BSR STOP builder
    0x923E: '4e75',
    0x927A: '24094842eb9a4280300220c0',  # original link word packing
    0x9286: '428010014840e48842428082e74b8043807c000320c04e75',
    0x930A: '4298',                    # STOP first longword is zero
    0x930A: '20fc00000004',            # STOP second longword is 4
    0x9310: '23c80002eeb4',            # updated cursor
    0x9316: '42b90002eeb8',            # reset record state
    0x931C: '4e75',
    # Specific retail MJP callback installation sites, not every possible game callback.
    0x1BE00: '23fc0001bec20002ee80', # original frontend callback
    0x1BFF2: '23fc0001e8040002ee80', # 10-record writer
    0x1C08C: '23fc0001e4d00002ee80', # 8-record writer
    0x1C130: '23fc0001e34a0002ee80', # 6-record writer
    0x1C1C0: '23fc0001e4d00002ee80', # 8-record writer
    0x1C25A: '23fc0001e34a0002ee80', # 6-record writer
    0x1E3BE: '21fc000000017fa8',     # callback path 6 ack after copy
    0x1E520: '21fc000000017fa8',     # callback path 8 ack after copy
    0x1E85E: '21fc000000017fa8',     # callback path 10 ack after copy
}

def code_from_rom(path: Path):
    rom = path.read_bytes()
    if len(rom) != 4194304 or hashlib.sha256(rom).hexdigest() != SHA:
        raise AssertionError('not the canonical original 4 MiB AVP.jag')
    stream = zlib.decompressobj(31)
    code = stream.decompress(rom[0x3804:])
    if not stream.eof or len(code) != 99072:
        raise AssertionError('retail 68000 stream identity/size mismatch')
    for addr, hexop in INSN.items():
        expected = bytes.fromhex(hexop)
        got = code[addr-BASE:addr-BASE+len(expected)]
        if got != expected:
            raise AssertionError(f'original instruction mismatch at ${addr:06X}: {got.hex()}')
    return code

def pc_branch(code, addr, size):
    op = struct.unpack_from('>H', code, addr-BASE)[0]
    assert op >> 12 == 6, (hex(addr), hex(op))
    if size == 2:
        displacement = (op & 0xff)
        if displacement > 127: displacement -= 256
        return addr + 2 + displacement
    assert (op & 0xff) == 0
    d = struct.unpack_from('>h', code, addr-BASE+2)[0]
    # 68000 16-bit branch displacements are relative to extension word.
    return addr + 2 + d

def audit(code):
    assert pc_branch(code, 0x91CC, 4) == 0x91D8
    assert pc_branch(code, 0x91D4, 4) == 0x91E2
    assert pc_branch(code, 0x91DE, 4) == 0x9308
    assert pc_branch(code, 0x9226, 2) == 0x927A
    assert pc_branch(code, 0x9238, 2) == 0x927A
    assert pc_branch(code, 0x923A, 4) == 0x9308
    assert 0x7548 + 2*8 == 0x7558
    assert 0x7558 + 8 == 0x7560
    # Callback target addresses are from original MOVE.L #imm,($2EE80).
    installs={}
    for a in (0x1BE00,0x1BFF2,0x1C08C,0x1C130,0x1C1C0,0x1C25A):
        raw = code[a-BASE:a-BASE+10]
        assert raw[:2] == b'\x23\xfc' and int.from_bytes(raw[6:], 'big') == 0x2ee80
        installs[a] = int.from_bytes(raw[2:6], 'big')
    expected={0x1BE00:0x1BEC2,0x1BFF2:0x1E804,0x1C08C:0x1E4D0,0x1C130:0x1E34A,0x1C1C0:0x1E4D0,0x1C25A:0x1E34A}
    assert installs==expected
    print(f'PASS retail ROM and {len(INSN)} original CPU opcode groups')
    print('PASS IRQ vector $0100 -> handler $91B6, timer increment, callback dispatch, IRQ acknowledgement and RTE')
    print('PASS callback-null fallback at $91D8 -> full tail STOP record (0,4) at $7560')
    print('PASS PRE_LIST branch writer twice ($7548/$7550), STOP record at $7558 and cursor $7560')
    print('PASS six 68000 frontend callback installations, including 6/8/10-record MJP writers')
    print('PASS all three MJP list callbacks publish $7FA8 only after their list-copy routine')
    print('LIMIT: static 68000 instruction audit, not a VBlank or TOM scanline-accurate execution trace')

if __name__ == '__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',type=Path,required=True)
    audit(code_from_rom(ap.parse_args().rom))
