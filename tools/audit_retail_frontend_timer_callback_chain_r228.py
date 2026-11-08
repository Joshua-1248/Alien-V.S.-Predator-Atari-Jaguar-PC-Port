#!/usr/bin/env python3
"""R228 ROM-only 68000 callback-chain verifier, no game instrumentation.

Establishes the original timer installation, wait, saved-callback invocation,
and restoration path. Static instruction evidence, not VBlank-cycle simulation.
The 1994 retail AVP.jag is needed locally; it is never copied into releases.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

ROM_SHA='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
BASE=0x8000
SLOT=0x2EE80
SAVED=0x2EF40
COUNT=0x2EF44
TIMERS=((0x9A5C,0x9ACA),(0x9A80,0x9AF6))


def original(path:Path)->bytes:
    rom=path.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA, 'incorrect retail ROM'
    obj=zlib.decompressobj(31)
    code=obj.decompress(rom[0x3804:])
    assert obj.eof and len(code)==99072
    return code


def raw(code:bytes,pc:int,size:int)->bytes:
    assert BASE<=pc and pc+size<=BASE+len(code)
    return code[pc-BASE:pc-BASE+size]


def exact(code:bytes,pc:int,expected_hex:str)->None:
    expected=bytes.fromhex(expected_hex)
    actual=raw(code,pc,len(expected))
    assert actual==expected,(f'${pc:06X}', actual.hex(),expected_hex)


def dest(code:bytes,pc:int,opcode:int)->int:
    instruction=struct.unpack('>H',raw(code,pc,2))[0]
    assert instruction==opcode,(hex(pc),hex(instruction),hex(opcode))
    if (opcode & 0xff)==0:
        disp=struct.unpack('>h',raw(code,pc+2,2))[0]
        return pc+2+disp  # 68000 displacement relative to extension-word address
    disp=opcode & 0xff
    return pc+2+(disp-256 if disp>=128 else disp)


def jsr(code:bytes,pc:int,target:int)->None:
    exact(code,pc,'4eb9'+struct.pack('>I',target).hex())


def audit(code:bytes)->None:
    # Timer A and timer B first call the *existing* pending-timer wait.
    # Their zero-duration branches skip BOTH the save and replacement writes.
    for start,target in TIMERS:
        exact(code,start,'6100'+struct.pack('>h',0x9AA4-(start+2)).hex())
        exact(code,start+4, '23c0'+struct.pack('>I',COUNT).hex())
        assert dest(code,start+10,0x6700)==start+0x22, 'zero-duration bypass to RTS'
        exact(code,start+14, '23f9'+struct.pack('>II',SLOT,SAVED).hex())
        exact(code,start+24, '23fc'+struct.pack('>II',target,SLOT).hex())
        exact(code,start+34, '4e75')
    # $9AA4 only blocks while a temporary timer callback is installed.
    exact(code,0x9AA4,'0cb9'+struct.pack('>II',0x9AF6,SLOT).hex())
    assert dest(code,0x9AAE,0x6700)==0x9AC0
    exact(code,0x9AB2,'0cb9'+struct.pack('>II',0x9ACA,SLOT).hex())
    assert dest(code,0x9ABC,0x6600)==0x9AC8
    exact(code,0x9AC0,'4ab9'+struct.pack('>I',COUNT).hex())
    assert dest(code,0x9AC6,0x66F8)==0x9AC0
    exact(code,0x9AC8,'4e75')
    # Timer A: read original JERRY DSP RAM word, subtract countdown;
    # on expiration clear COUNT and restore original callback.
    exact(code,0x9ACA,'203900f1bb40')
    exact(code,0x9AD0,'90b9'+struct.pack('>I',COUNT).hex())
    assert dest(code,0x9AD6,0x6A00)==0x9AEC
    exact(code,0x9ADA,'4280')
    exact(code,0x9ADC,'42b9'+struct.pack('>I',COUNT).hex())
    exact(code,0x9AE2,'23f9'+struct.pack('>II',SAVED,SLOT).hex())
    exact(code,0x9AEC,'23c000f1bb40')
    assert dest(code,0x9AF2,0x6000)==0x9B24
    # Timer B: add countdown; on expiration set the original source's
    # $7FFF sentinel and restore the original saved callback.
    exact(code,0x9AF6,'203900f1bb40')
    exact(code,0x9AFC,'d0b9'+struct.pack('>I',COUNT).hex())
    exact(code,0x9B02,'4a40')
    assert dest(code,0x9B04,0x6A00)==0x9B1E
    exact(code,0x9B08,'203c00007fff')
    exact(code,0x9B0E,'42b9'+struct.pack('>I',COUNT).hex())
    exact(code,0x9B14,'23f9'+struct.pack('>II',SAVED,SLOT).hex())
    exact(code,0x9B1E,'23c000f1bb40')
    # BOTH timer callbacks converge at $9B24 and invoke the SAVED
    # callback after timer bookkeeping, unless the saved pointer is null.
    exact(code,0x9B24,'2039'+struct.pack('>I',SAVED).hex())
    assert dest(code,0x9B2A,0x6700)==0x9B32
    exact(code,0x9B2E,'20404e90')  # MOVEA.L D0,A0 / JSR (A0)
    exact(code,0x9B32,'4e75')
    # Original frontend mode 1 first installs 8-record list callback and
    # runs $9AA4, later calls $9A80, and finally calls $9AA4 again.
    exact(code,0x1C08C,'23fc'+struct.pack('>II',0x1E4D0,SLOT).hex())
    jsr(code,0x1C096,0x9AA4)
    jsr(code,0x1C0B0,0x9A80)
    jsr(code,0x1C0BC,0x9AA4)
    # Original mode 3 also arms timer B before nested resource setup.
    exact(code,0x1C1C0,'23fc'+struct.pack('>II',0x1E4D0,SLOT).hex())
    jsr(code,0x1C1F4,0x9A80)
    jsr(code,0x1C1FA,0x1C4E0)
    # Three other retail call sites arm timer A; their FULL dynamic path
    # may invoke additional callbacks or early exits, so don't infer timing.
    for pc in (0x1CE16,0x1EF4E,0x1F88A):
        jsr(code,pc,0x9A5C)
    # First tick in the dispatcher precedes mode callback installation.
    exact(code,0x1BEC4,'21c07fac')
    jsr(code,0x1BEC8,0x91F6)
    print('PASS retail 68000 image and original callback-chaining instruction checks')
    print('PASS timer A/B save original callback and install only for nonzero countdown')
    print('PASS temporary-callback wait, both restore paths, shared saved-callback JSR')
    print('PASS mode 1 and 3 timer B calls, three original timer A call sites')
    print('PASS mode dispatch performs One_Tick BEFORE the mode-specific callback install')
    print('LIMIT: no full dynamic IRQ/scanline trace, TOM bus-order guarantee, or pixel parity claim')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    audit(original(p.parse_args().rom))
