#!/usr/bin/env python3
"""Source-to-retail comparison of 1994 AVP character-selection score entry.

Consumes the user's own unmodified 4-MiB retail cartridge. Never exports ROM,
patches, score events, oscillator data, WAVs, or proprietary synthesis tables.
"""
import argparse
import hashlib
import struct
import sys
import zlib
from pathlib import Path

SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'

def u16(blob, pos): return struct.unpack_from('>H', blob, pos)[0]
def u32(blob, pos): return struct.unpack_from('>I', blob, pos)[0]

def check(blob, cpu):
    # These mapped addresses are for the authenticated final ROM and
    # represent the CPU-side call ordering, not a host volume approximation.
    base = 0x8000
    steps = [
        (0x1C096, 0x009AA4, 'wait previous fade'),
        (0x1C0A4, 0x0099B2, 'start selection score'),
        (0x1C0B0, 0x009A80, 'begin original fade-up'),
        # R277: the character-select carousel itself runs DURING the fade.
        (0x1C0B6, 0x01E868, 'Do_Selec carousel (runs while master fade rises)'),
        (0x1C0BC, 0x009AA4, 'wait fade completion'),
    ]
    for at, destination, name in steps:
        off = at-base
        if cpu[off:off+2] != b'\x4e\xb9' or u32(cpu, off+2) != destination:
            raise ValueError(f'{name} not found at mapped address ${at:06X}')
    if u16(cpu, 0x1C09C-base)!=0x207C or u32(cpu, 0x1C09E-base)!=0x2046C:
        raise ValueError('selection music descriptor load is not $2046C')
    if u16(cpu, 0x1C0AA-base)!=0x203C or u32(cpu, 0x1C0AC-base)!=0xFA:
        raise ValueError('fade-up is not the original $FA step')
    # Decode original runtime data for structural checks only.
    data = zlib.decompressobj(31).decompress(blob[0xE840:])
    txt = zlib.decompressobj(31).decompress(blob[0x3804:])
    desc = 0x2046C - 0x20300
    score_cell=u32(data, desc)
    patch_addr=u32(data, desc+4)
    if score_cell != 0x80815F14 or patch_addr != 0x20480:
        raise ValueError('unexpected selection music header')
    score_ptr=u32(blob, (score_cell&0x7FFFFFFF)-0x800000)
    if score_ptr!=0xAF5584:
        raise ValueError('unexpected selection score target')
    so=score_ptr-0x800000
    notes=[]
    for i in range(32):
        when, ev=u32(blob,so+8*i),u32(blob,so+8*i+4)
        if when==0x7FFFFFFF:break
        if ev & 0x80000000:
            notes.append((when,(ev>>26)&7,(ev>>21)&31,ev&0x7F))
    if notes[:4] != [(125,0,2,127),(125,1,1,127),
                    (125,2,1,127),(125,3,2,127)]:
        raise ValueError('unexpected opening score events')
    for patch in (1,2):
        p = patch_addr-0x20300 + 80*patch
        if u32(data,p)!=4 or u32(data,p+44)!=24 or u32(data,p+56)!=0:
            raise ValueError('opening patch is not FM with zero-phase attack')
        envelope_addr=u32(data,p+40)
        t=0x9E98-0x8000 + envelope_addr-0xF1BC00
        if u32(txt,t)!=0 or u32(txt,t+4)!=0x7FFF:
            raise ValueError('opening envelope does not rise from zero')
    print('ROM SHA256: authenticated retail')
    print('Mode-1 order: fade wait -> StartMus($2046C) -> FXFadeUp($FA) -> Do_Selec($1E868) -> fade wait: PASS')
    print('Initial selection notes: four simultaneous full-velocity FM notes at score tick 125')
    print('First-note patches: FM types 1/2; envelope phase 0; first table levels 0 -> $7FFF')
    print('Original master ramp: 132 callbacks from silence to $7FFF (when starting at zero)')
    print('Conclusion: score and per-note attacks advance DURING the master fade')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',type=Path,required=True)
    args=ap.parse_args()
    data=args.rom.read_bytes()
    if len(data)!=4194304 or hashlib.sha256(data).hexdigest()!=SHA:
        sys.exit('Wrong ROM: expected the authenticated retail 4-MiB cartridge')
    cpu=zlib.decompressobj(31).decompress(data[0x3804:])
    try: check(data,cpu)
    except (ValueError,struct.error,zlib.error) as exc:sys.exit(str(exc))
if __name__=='__main__':main()
