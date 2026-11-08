#!/usr/bin/env python3
"""R227 read-only retail AVP frontend dispatch/callback-handoff audit.

Verifies source opcode and *bounded static control-flow* relationships against
an independently supplied commercial retail ROM. Does not execute a 68000 or
make timing / scanline / GPU-visibility claims. No game binary instrumentation.
"""
from __future__ import annotations
import argparse
from hashlib import sha256
from pathlib import Path
import struct
import zlib

ORIGINAL_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
BASE = 0x8000
SLOT = 0x2EE80
COMMON_EXIT = 0x1C36E
# 9 mutually exclusive modes: compare address, index, failed-test destination,
# callback installation, installed handler, successful-case terminal BRA.W.
CASES = (
    (0x1BF52, 0, 0x1C006, 0x1BFF2, 0x1E804, 0x1C002),
    (0x1C006, 1, 0x1C0C6, 0x1C08C, 0x1E4D0, 0x1C0C2),
    (0x1C0C6, 2, 0x1C144, 0x1C130, 0x1E34A, 0x1C140),
    (0x1C144, 3, 0x1C204, 0x1C1C0, 0x1E4D0, 0x1C200),
    (0x1C204, 4, 0x1C26E, 0x1C25A, 0x1E34A, 0x1C26A),
    (0x1C26E, 5, 0x1C2AE, 0x1C29A, 0x1E34A, 0x1C2AA),
    (0x1C2AE, 6, 0x1C2EE, 0x1C2DA, 0x1E34A, 0x1C2EA),
    (0x1C2EE, 7, 0x1C32E, 0x1C31A, 0x1E34A, 0x1C32A),
    (0x1C32E, 8, 0x1C36E, 0x1C35A, 0x1E34A, 0x1C36A),
)

def retail_code(rom_path:Path)->bytes:
    rom=rom_path.read_bytes()
    assert len(rom)==4_194_304 and sha256(rom).hexdigest()==ORIGINAL_SHA, 'wrong retail cartridge'
    unzip=zlib.decompressobj(31)
    code=unzip.decompress(rom[0x3804:])
    assert unzip.eof and len(code)==99072, 'unexpected 68000 image'
    return code

def at(code:bytes,pc:int,count:int)->bytes:
    offset=pc-BASE
    assert offset>=0 and offset+count<=len(code)
    return code[offset:offset+count]

def signed_branch(code:bytes,pc:int,condition:int)->int:
    op=struct.unpack('>H',at(code,pc,2))[0]
    assert (op&0xff00)==condition, (hex(pc),hex(op),hex(condition))
    displacement=struct.unpack('>h',at(code,pc+2,2))[0]
    return pc+2+displacement

def callback_at(code:bytes,pc:int)->int:
    op=at(code,pc,10)
    assert op[:2]==b'\x23\xfc' and int.from_bytes(op[6:10],'big')==SLOT,hex(pc)
    return int.from_bytes(op[2:6],'big')

def audit(code:bytes)->None:
    # These are distinct *original functions*: R225's no-op initializer at
    # 1BE00 and the dispatch/setup entry beginning at 1BEC4.
    assert callback_at(code,0x1BE00)==0x1BEC2
    assert at(code,0x1BE0A,6)==bytes.fromhex('4eb9000091f6')
    assert at(code,0x1BEC0,4)==bytes.fromhex('4e754e75')
    assert at(code,0x1BEC4,4)==bytes.fromhex('21c07fac'), 'mode index stored to 7FAC'
    assert at(code,0x1BEC8,6)==bytes.fromhex('4eb9000091f6'), 'mode-index write followed by One_Tick'
    assert at(code,COMMON_EXIT,2)==bytes.fromhex('4e75'), 'shared return instruction'
    for cmp_addr,mode,fail,install,target,exit_branch in CASES:
        compare=at(code,cmp_addr,8)
        assert compare[:2]==bytes.fromhex('0cb8') and int.from_bytes(compare[2:6],'big')==mode
        assert compare[6:8]==bytes.fromhex('7fac')
        assert signed_branch(code,cmp_addr+8,0x6600)==fail, (hex(cmp_addr),'false-path')
        assert callback_at(code,install)==target
        assert signed_branch(code,exit_branch,0x6000)==COMMON_EXIT, (hex(mode),'success return')
        # R226 independently scanned all constant callback-slot writes. Each
        # case contains exactly its own *direct encoded installation*, and
        # a successful case cannot fall through into another case's installer.
        lo=install+10
        assert exit_branch>=lo
        for nxt in range(lo,exit_branch+1,2):
            if at(code,nxt,2)==bytes.fromhex('23fc'):
                v=at(code,nxt,10)
                assert int.from_bytes(v[6:10],'big')!=SLOT,(mode,hex(nxt))
    # Mode 3 has two early shortcuts to the SAME callback setup, not to the
    # common exit. Its later resource-setup conditional likewise rejoins at
    # its own mode-specific JSR. These are static branch assertions only.
    for early in (0x1C18C,0x1C1A6):
        assert signed_branch(code,early,0x6000)==0x1C1C0,hex(early)
    assert at(code,0x1C1CA,8)==bytes.fromhex('0c3900020004be69')
    assert signed_branch(code,0x1C1D2,0x6600)==0x1C1FA
    assert at(code,0x1C1FA,6)==bytes.fromhex('4eb90001c4e0')
    assert at(code,0x1C1F8,2)!=b'\x4e\x75'
    # Distinguish the interrupt call from screen visibility: IRQ invokes the
    # installed handler; the wait instruction resumes following IRQ wakeup.
    assert at(code,0x91C6,6)==bytes.fromhex('20390002ee80')
    assert at(code,0x91D0,4)==bytes.fromhex('20404e90')
    assert at(code,0x91F6,8)==bytes.fromhex('33fcffff0002ee88')
    assert at(code,0x91FE,4)==bytes.fromhex('4e722000')
    for callback,ack in ((0x1E34A,0x1E3BE),(0x1E4D0,0x1E520),(0x1E804,0x1E85E)):
        assert ack>callback
        assert at(code,ack,8)==bytes.fromhex('21fc000000017fa8')
    print('PASS retail 4MiB cartridge identity and 99072-byte decompressed CPU program')
    print('PASS distinct no-op initializer and mode-setup entry; each has original One_Tick call')
    print('PASS nine indexed dispatch comparisons, nine BNE.W false targets')
    print('PASS nine callback installations, nine BRA.W successful-case returns to $1C36E')
    print('PASS mode-3 early rejoin paths and post-install conditional to original JSR')
    print('PASS callback function identities and three post-copy $7FA8 acknowledgments')
    print('LIMIT: static direct-branch coverage; subroutine and indirect callback writes NOT executed')
    print('LIMIT: no 68000 instruction-cycle trace, TOM scanline fetch, or displayed CROSS parity')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',required=True,type=Path)
    audit(retail_code(parser.parse_args().rom))
