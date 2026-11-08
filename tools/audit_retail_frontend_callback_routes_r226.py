#!/usr/bin/env python3
"""R226 read-only audit of retail MJP frontend callback dispatch.

This checks original 68000 instruction operands, nine mode tests and their
false-branch destinations, callback installer immediates and bounded follow-up
JSRs. It does NOT simulate runtime interrupts, OP frame fetches or GPU timing.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
BASE = 0x8000
SLOT = 0x2ee80
# Pre-dispatch no-op, then mode 0..8. The preceding R225 audit enumerated only
# the first six of these sites. The final four occupy $1C29A-$1C35A.
FRONTEND = [
    (0x1BE00, 0x1BEC2, None, 'pre-dispatch idle'),
    (0x1BFF2, 0x1E804, 0x1F118, 'default / mode 0'),
    (0x1C08C, 0x1E4D0, 0x9AA4, 'mode 1'),
    (0x1C130, 0x1E34A, 0x1D846, 'mode 2'),
    (0x1C1C0, 0x1E4D0, None, 'mode 3'),
    (0x1C25A, 0x1E34A, 0x1DB78, 'mode 4'),
    (0x1C29A, 0x1E34A, 0x1FED8, 'mode 5'),
    (0x1C2DA, 0x1E34A, 0x1FED8, 'mode 6'),
    (0x1C31A, 0x1E34A, 0x1DA06, 'mode 7'),
    (0x1C35A, 0x1E34A, 0x1DA06, 'mode 8'),
]
# Tests on ($7FAC) dispatch modes 1..8. A failed comparison (BNE.W)
# skips to the next mode test, not to a callback installer.
MODE_TESTS = [
    (0x1BF52, 0, 0x1C006),
    (0x1C006, 1, 0x1C0C6),
    (0x1C0C6, 2, 0x1C144),
    (0x1C144, 3, 0x1C204),
    (0x1C204, 4, 0x1C26E),
    (0x1C26E, 5, 0x1C2AE),
    (0x1C2AE, 6, 0x1C2EE),
    (0x1C2EE, 7, 0x1C32E),
    (0x1C32E, 8, 0x1C36E),
]

# Other direct constant callback writes in the same retail decompressed 68000
# program. Do not infer that the MJP callback lives forever after installation.
OTHER_IMMEDIATE = {
    0x88EE: 0x92D0, 0x8974: 0x92D0, 0x8FE4: 0x9020,
    0x9A74: 0x9ACA, 0x9A98: 0x9AF6,
    0xC79E: 0xC7E2, 0xC7D8: 0xC8C2, 0xC7EE: 0xC8C0,
    0xC8B6: 0xC8C2, 0x1389A: 0x92D0,
}
COPY_WRITES = {0x9AE2: 0x2EF40, 0x9B14: 0x2EF40}
ZERO_WRITES = (0x88B0, 0x917E, 0x13830)

def code_from_rom(path: Path) -> bytes:
    rom = path.read_bytes()
    assert len(rom) == 4_194_304 and hashlib.sha256(rom).hexdigest() == SHA, 'wrong retail ROM'
    gz = zlib.decompressobj(31)
    code = gz.decompress(rom[0x3804:])
    assert gz.eof and len(code) == 99_072, 'incorrect original 68000 code'
    return code

def data_at(code: bytes, pc: int, n: int) -> bytes:
    assert BASE <= pc and pc-BASE+n <= len(code)
    return code[pc-BASE:pc-BASE+n]

def installer_at(code: bytes, pc: int) -> int:
    raw = data_at(code, pc, 10)
    assert raw[:2] == b'\x23\xfc', f'expected MOVE.L #imm to absolute long at {pc:x}'
    assert int.from_bytes(raw[6:10], 'big') == SLOT
    return int.from_bytes(raw[2:6], 'big')

def audit(code: bytes) -> None:
    for pc, target, jsr, label in FRONTEND:
        assert installer_at(code, pc) == target, (hex(pc), hex(target), label)
        if jsr is not None:
            assert data_at(code, pc+10, 6) == b'\x4e\xb9' + struct.pack('>I', jsr), (hex(pc),label)
    assert data_at(code, 0x1BE0A, 6) == bytes.fromhex('4eb9000091f6')
    assert data_at(code, 0x1BEC2, 2) == b'\x4e\x75'
    assert data_at(code, 0x1C1CA, 2) == b'\x0c\x39', 'mode 3 has a condition after installer, not an immediate JSR'
    for cmp_pc, mode, fail in MODE_TESTS:
        raw = data_at(code, cmp_pc, 12)
        assert raw[:2] == b'\x0c\xb8' and int.from_bytes(raw[2:6],'big') == mode
        assert raw[6:8] == b'\x7f\xac', (hex(cmp_pc),'mode field')
        assert raw[8:10] == b'\x66\x00', (hex(cmp_pc),'expected BNE.W')
        dest = cmp_pc + 10 + struct.unpack('>h', raw[10:12])[0]
        assert dest == fail, (hex(cmp_pc), hex(dest), hex(fail))
    for pc, val in OTHER_IMMEDIATE.items():
        assert installer_at(code, pc) == val, (hex(pc), hex(val))
    for pc, src in COPY_WRITES.items():
        raw=data_at(code,pc,10)
        assert raw[:2]==b'\x23\xf9' and int.from_bytes(raw[2:6],'big')==src
        assert int.from_bytes(raw[6:10],'big')==SLOT
    for pc in ZERO_WRITES:
        assert data_at(code,pc,6)==b'\x42\xb9'+struct.pack('>I',SLOT)
    # Exhaustively enumerate occurrences of these THREE *specific* encodings
    # in this verified program image, not every possible indirect callback write.
    def scan(kind: bytes, length: int, offset: int, selected: set[int]) -> None:
        matches=set()
        start=0
        while True:
            i=code.find(kind,start)
            if i<0:break
            if i+length<=len(code) and code[i+offset:i+offset+4]==struct.pack('>I',SLOT):
                matches.add(i+BASE)
            start=i+1
        assert matches==selected, ('unaccounted matching writes', kind.hex(), sorted(matches-selected),sorted(selected-matches))
    scan(b'\x23\xfc',10,6,{p for p,_,_,_ in FRONTEND}|set(OTHER_IMMEDIATE))
    scan(b'\x23\xf9',10,6,set(COPY_WRITES))
    scan(b'\x42\xb9',6,2,set(ZERO_WRITES))
    assert [v for _,v,_,_ in FRONTEND].count(0x1E34A)==6
    print('PASS canonical original retail ROM and decompressed 68000 program')
    print('PASS 10 frontend direct callback installations: idle plus mode-0..mode-8 routes')
    print('PASS nine mode tests on ($7FAC), including exact BNE.W next-mode targets')
    print('PASS direct post-install JSR target checks and mode-3 conditional exception')
    print('PASS program-wide enumeration of matching encoding classes: 20 direct, 2 copy, 3 clear writes')
    print('PASS original 6/8/10-list callback identities retained; no-op remains distinct')
    print('LIMIT: other indirect writers and dynamic callback lifetimes are not ruled out')
    print('LIMIT: no original VBlank scanline timing, TOM OP fetch timing or displayed frame parity')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',required=True,type=Path)
    audit(code_from_rom(parser.parse_args().rom))
