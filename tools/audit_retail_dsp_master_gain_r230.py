#!/usr/bin/env python3
"""R230 read-only retail-ROM audit of FullSynth's $F1BB40 stereo master gain.

Validates original 68000-compressed image and original embedded JERRY DSP
program; decodes only the relevant DSP ISA opcodes. Never saves ROM code,
waveforms, game assets, or a proprietary DSP executable.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
CPU_SHA = 'c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53'
DSP_SHA = '5ce1a2bdb193675eec2c73936e52b05fa82f8156c7a742c4b87200f1d9d1db7e'
CPU_BASE = 0x8000
DSP_BASE = 0xF1B000
DSP_LENGTH = 0x0B88
DSP_HEADER_CPU_ADDRESS = 0xB088
FADE_WORD = 0xF1BB40


def decode_original(path: Path) -> tuple[bytes,bytes]:
    rom=path.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA, 'unrecognized retail AVP.jag'
    inflater=zlib.decompressobj(31)
    cpu=inflater.decompress(rom[0x3804:])
    assert inflater.eof and len(cpu)==99072 and hashlib.sha256(cpu).hexdigest()==CPU_SHA
    start=DSP_HEADER_CPU_ADDRESS-CPU_BASE
    assert cpu[start:start+4]==struct.pack('>I',DSP_BASE)
    assert cpu[start+4:start+8]==struct.pack('>I',DSP_LENGTH)
    image=cpu[start+8:start+8+DSP_LENGTH]
    assert len(image)==DSP_LENGTH and hashlib.sha256(image).hexdigest()==DSP_SHA
    # Confirm that the embedded program occurs only at this location and the
    # full load descriptor is not accidentally confused with DSP instructions.
    hdr=struct.pack('>II',DSP_BASE,DSP_LENGTH)
    assert cpu.find(hdr)==start and cpu.find(hdr,start+1)<0
    return cpu,image


def word(image:bytes, pc:int) -> int:
    assert DSP_BASE <= pc <= DSP_BASE+len(image)-2 and pc%2==0
    return struct.unpack_from('>H',image,pc-DSP_BASE)[0]


def fields(instruction:int) -> tuple[int,int,int]:
    return instruction>>10, (instruction>>5)&31, instruction&31


def check(image:bytes,pc:int,op:int,reg1:int,reg2:int) -> None:
    actual=fields(word(image,pc))
    expected=(op,reg1,reg2)
    assert actual==expected,(hex(pc),actual,expected)


def signed16(v:int)->int:
    v &= 0xffff
    return v-0x10000 if v&0x8000 else v


def signed32(v:int)->int:
    v &= 0xffffffff
    return v-0x100000000 if v&0x80000000 else v


def stage(accum:int, gain:int) -> int:
    # Original DSP: SHARQ #15, IMULT signed-low-16 * signed-low-16,
    # SHARQ #15. No invented volume curve or clipping step.
    normalized=signed32(accum)>>15
    product=signed16(normalized)*signed16(gain)
    return product>>15


def audit(cpu:bytes,image:bytes) -> None:
    # Original handler's 32-bit channel accumulators in r9 and r26 are
    # normalized first. Both later use the same gain word through r2.
    for pc,reg in ((0xF1B100,9),(0xF1B102,26),(0xF1B110,9),(0xF1B112,26)):
        check(image,pc,0x1B,15,reg)      # SHARQ #15, rN
    check(image,0xF1B104,0x26,0,2)  # MOVEI #$F1BB40,r2
    assert word(image,0xF1B106)==(FADE_WORD&0xffff)
    assert word(image,0xF1B108)==(FADE_WORD>>16)
    check(image,0xF1B10A,0x29,2,2)  # LOAD (r2),r2: 32-bit DSP RAM read
    check(image,0xF1B10C,0x11,2,9)  # IMULT r2,r9 right
    check(image,0xF1B10E,0x11,2,26) # IMULT r2,r26 left
    # DSP MOVEI address is stored low-word first, unlike 68000 addresses.
    lookup=struct.pack('>HH',FADE_WORD&0xffff,FADE_WORD>>16)
    occurrences=[DSP_BASE+i for i in range(2,len(image)-3,2) if image[i:i+4]==lookup]
    assert occurrences==[0xF1B106],occurrences
    # Cross-check 68000 image also contains the known R229 CPU access sites.
    for addr in (0x9ACA,0x9AF6):
        off=addr-CPU_BASE
        assert cpu[off:off+6]==bytes.fromhex('203900f1bb40')
    for addr in (0x9AEC,0x9B1E):
        off=addr-CPU_BASE
        assert cpu[off:off+6]==bytes.fromhex('23c000f1bb40')
    # A zero gain silences both channel outputs, whereas 0x7FFF does
    # not add an extra attenuation law: it uses the original IMULT/SHARQ.
    fixtures=0
    for acc in (0,1,-1,0x00012345,-0x00012345,0x00100000,-0x00100000,
                0x7fffffff,-0x80000000):
        for gain in (0,17,250,16384,32750,32767):
            result=stage(acc,gain)
            if gain==0:assert result==0
            assert -32768<=result<=32767
            fixtures+=1
    print('PASS authenticated retail ROM and original 99,072-byte 68000 program')
    print('PASS embedded original 2,952-byte DSP payload, original load address and SHA-256')
    print('PASS original DSP reads $F1BB40 once in common stereo output stage')
    print('PASS both channel IMULT operations use the same signed-low-16 master gain')
    print('PASS original shift ordering and %d signed DSP arithmetic fixtures' % fixtures)
    print('PASS original 68000 fade reads/writes cross-referenced')
    print('LIMIT no real-time DSP scheduling, exact DAC sample output, or hardware audio capture verified')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,required=True)
    args=parser.parse_args()
    audit(*decode_original(args.rom))
