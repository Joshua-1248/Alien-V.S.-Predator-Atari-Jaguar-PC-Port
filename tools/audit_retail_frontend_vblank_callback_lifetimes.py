#!/usr/bin/env python3
"""R225 read-only retail Jaguar 68000 frontend VBlank callback and wait audit.

Authenticates original ROM and the exact 68000 instructions (not PC port code),
then traces the callback slot's *possible* targets across the six documented
MJP install sites. A static audit does not establish realtime phase durations,
VBlank latency, or actual GPU/OP pixel publication.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
CPU_BASE = 0x8000
# All windows are nonoverlapping, keyed by exact first opcode address.
INSTRUCTIONS = {
    0x918A: '21fc000091b60100',    # Install $91B6 in VBlank exception vector $0100.
    0x91B6: '52b90002ee84',        # ADDQ.L #1,$2EE84 (VBlank counter)
    0x91BC: '42790002ee88',        # CLR.W $2EE88 (One_Tick wake sentinel)
    0x91C2: '48e7fffe',            # MOVEM.L preserve
    0x91C6: '20390002ee80',        # MOVE.L ($2EE80),D0 (installed callback)
    0x91CC: '6700000a',            # BEQ $91D8 (null callback)
    0x91D0: '20404e90',            # MOVEA.L D0,A0; JSR (A0)
    0x91D4: '6000000c',            # BRA $91E2 (skip null fallback)
    0x91D8: '207c00007560',        # MOVEA.L #$7560,A0
    0x91DE: '61000128',            # BSR STOP builder $9308
    0x91E2: '4cdf7fff',            # Restore
    0x91E6: '33fc010100f000e0',    # acknowledge IRQ
    0x91EE: '427900f000e2',        # TOM interrupt clear
    0x91F4: '4e73',                # RTE
    0x91F6: '33fcffff0002ee88',    # One_Tick: set wake sentinel -1
    0x91FE: '4e722000',            # STOP #$2000, wait for interrupt
    0x9202: '4a790002ee88',        # TST.W $2EE88
    0x9208: '66f4',                # BNE $91FE until woken
    0x920A: '4e75',                # RTS
    0x9308: '4298',                # Fallback STOP phrase first longword
    0x930A: '20fc00000004',        # second longword =4
    0x1BE00: '23fc0001bec20002ee80', # install immediate-return frontend callback
    0x1BE0A: '4eb9000091f6',     # JSR One_Tick after installation
    0x1BEC0: '4e75',              # preceding frontend routine return
    0x1BEC2: '4e75',              # ENTIRE callback body: RTS
    0x1BEC4: '21c07fac',          # next *separate* routine begins; not callback!
    0x1BFF2: '23fc0001e8040002ee80', # 10-record route
    0x1C08C: '23fc0001e4d00002ee80', # 8-record route
    0x1C130: '23fc0001e34a0002ee80', # 6-record route
    0x1C1C0: '23fc0001e4d00002ee80', # 8-record route
    0x1C25A: '23fc0001e34a0002ee80', # 6-record route
    0x1E34A: '247c0004fc00',      # 6-record path starts with A2=source list
    0x1E3BE: '21fc000000017fa8', # 6-record path publishes GPU ack after copy
    0x1E4D0: '247c0004fc00',      # 8-record path starts with A2=source list
    0x1E520: '21fc000000017fa8', # 8-record ack
    0x1E804: '247c0004fc00',      # 10-record path starts with A2=source list
    0x1E85E: '21fc000000017fa8', # 10-record ack
}

INSTALLS = (0x1BE00, 0x1BFF2, 0x1C08C, 0x1C130, 0x1C1C0, 0x1C25A)
TARGETS = (0x1BEC2, 0x1E804, 0x1E4D0, 0x1E34A, 0x1E4D0, 0x1E34A)

def program(path: Path) -> bytes:
    rom = path.read_bytes()
    assert len(rom)==4_194_304 and hashlib.sha256(rom).hexdigest()==ROM_SHA, 'incorrect ROM'
    gz = zlib.decompressobj(31)
    code=gz.decompress(rom[0x3804:])
    assert gz.eof and len(code)==99_072
    for pc, expected_hex in INSTRUCTIONS.items():
        expected=bytes.fromhex(expected_hex)
        actual=code[pc-CPU_BASE:pc-CPU_BASE+len(expected)]
        assert actual==expected,(hex(pc),actual.hex(),expected_hex)
    return code

def signed_branch(code:bytes, pc:int)->int:
    op=struct.unpack_from('>H',code,pc-CPU_BASE)[0]
    assert (op>>12)==6
    displacement=op&0xff
    if displacement:
        if displacement>=128:displacement-=256
        return pc+2+displacement
    displacement=struct.unpack_from('>h',code,pc-CPU_BASE+2)[0]
    return pc+2+displacement

def audit(code: bytes):
    assert signed_branch(code,0x91CC)==0x91D8
    assert signed_branch(code,0x91D4)==0x91E2
    assert signed_branch(code,0x91DE)==0x9308
    assert signed_branch(code,0x9208)==0x91FE
    assert code[0x1BEC2-CPU_BASE:0x1BEC4-CPU_BASE]==b'\x4e\x75'
    assert code[0x1BEC4-CPU_BASE:0x1BEC6-CPU_BASE]!=b'\x4e\x75'
    installs=[]
    for pc,expected in zip(INSTALLS,TARGETS):
        raw=code[pc-CPU_BASE:pc-CPU_BASE+10]
        assert raw[:2]==b'\x23\xfc'
        callback=int.from_bytes(raw[2:6],'big')
        slot=int.from_bytes(raw[6:10],'big')
        assert callback==expected and slot==0x2EE80
        installs.append(callback)
    assert installs==list(TARGETS)
    assert installs.count(0x1BEC2)==1
    assert [installs.count(x) for x in (0x1E804,0x1E4D0,0x1E34A)]==[1,2,2]
    # The front-end no-op callback doesn't acknowledge $7FA8 or write a list;
    # only non-null list writer routines do. The interrupt code's own VBlank
    # counter and wake flag updates occur independently of the callback body.
    assert code[0x91B6-CPU_BASE:0x91C2-CPU_BASE]==bytes.fromhex('52b90002ee8442790002ee88')
    for start, ack in ((0x1E34A,0x1E3BE),(0x1E4D0,0x1E520),(0x1E804,0x1E85E)):
        assert ack>start
        assert code[ack-CPU_BASE:ack-CPU_BASE+8]==bytes.fromhex('21fc000000017fa8')
    print(f'PASS original retail ROM; {len(INSTRUCTIONS)} disjoint verified 68000 instruction groups')
    print('PASS $91B6 VBlank handler: frame counter and wake flag update before callback dispatch')
    print('PASS $91F6 One_Tick: STOP #$2000 / $2EE88 wait loop; 68000 branch verified')
    print('PASS frontend $1BE00 installs $1BEC2; callback body is EXACTLY RTS, zero list operations')
    print('PASS six original callback installation sites: no-op, 10, 8, 6, 8, 6 record writers')
    print('PASS real list callbacks ACK $7FA8 following their copy blocks; no-op does not')
    print('PASS null callback $91D8 uses fallback STOP writer, which differs from no-op callback')
    print('LIMIT: static instruction/code-flow identity; no claims about dynamic branch choice,')
    print('       hardware-cycle VBlank dispatch, bus arbitration, or displayed frame parity')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    audit(program(p.parse_args().rom))
