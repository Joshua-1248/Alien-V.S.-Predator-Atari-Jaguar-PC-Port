#!/usr/bin/env python3
"""Verify the original Jaguar MJP title-logo memory bindings and source buffers.

Reads the user's retail ROM only; extracts no commercial ROM graphics in output.
Checks that the ROM's 68000 loader binds FILES 236/237 to the exact pointer
variables read by Make_TLi, and that the GZIP resources have the original
320-pixel RGB16 layout. This does NOT certify OP temporal/fractional scaling.
"""
import argparse
from pathlib import Path
import hashlib
import zlib

SHA='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
EXPECTED={236:(320,122,0x4BF40),237:(320,76,0x4BF44)}

def main(path):
    rom=path.read_bytes()
    if len(rom)!=4_194_304 or hashlib.sha256(rom).hexdigest()!=SHA:
        raise ValueError('Not the verified retail AVP.jag')
    d=zlib.decompressobj(31)
    cpu=d.decompress(rom[0x3804:])
    if not d.eof or len(cpu)!=99072:
        raise ValueError('Original retail 68000 stream is incorrect')
    for file_id,(w,h,ptr) in EXPECTED.items():
        # MOVE.W #file_id,D0; MOVEA.L #pointer_slot,A2; JSR LoadFile.
        instruction=(b'\x30\x3c'+file_id.to_bytes(2,'big')+
                     b'\x24\x7c'+ptr.to_bytes(4,'big')+b'\x4e\xb9')
        hits=[i for i in range(len(cpu)) if cpu.startswith(instruction,i)]
        if len(hits)!=1 or not 0x1BF70<=hits[0]+0x8000<=0x1BFB0:
            raise ValueError(f'FILES {file_id}: original loader binding missing')
        # Make_TLi MOVE.L (pointer),Ob_Data: reads the loaded buffer.
        make_tli=b'\x23\xf9'+ptr.to_bytes(4,'big')+bytes.fromhex('0004beec')
        if not 0x1E530<=cpu.find(make_tli)+0x8000<=0x1E610:
            raise ValueError(f'FILES {file_id}: original OP bind missing')
        print(f'CPU ${hits[0]+0x8000:06X}: load FILES {file_id} -> ${ptr:06X}; Make_TLi binds OP')
    off=0x2F8178; actual={}
    for index in range(240):
        tag=rom[off:off+4];size=int.from_bytes(rom[off+4:off+8],'big');end=off+8+size
        if end>len(rom):raise ValueError('FILE list overflow')
        if index in EXPECTED:
            w,h,_=EXPECTED[index]
            if tag!=b'GZIP':raise ValueError(f'FILES {index} was not original GZIP')
            raw=zlib.decompress(rom[off+8:end],31)
            if len(raw)!=2*w*h:raise ValueError(f'FILES {index} unexpected RGB16 layout')
            nonzero=[]
            for y in range(h):
                for x in range(w):
                    pos=2*(y*w+x)
                    if raw[pos]!=0 or raw[pos+1]!=0:nonzero.append((x,y))
            minx=min(x for x,y in nonzero);maxx=max(x for x,y in nonzero)
            miny=min(y for x,y in nonzero);maxy=max(y for x,y in nonzero)
            actual[index]=(w,h,len(nonzero),(minx,miny,maxx,maxy))
        off=(end+3)&~3
    for i,(w,h,n,box) in actual.items():
        print(f'FILES {i}: {w}x{h} RGB16, opaque/ink={n} pixels, nonzero bbox={box}')
    assert actual[236][3]==(12,22,311,114)
    assert actual[237][3]==(6,6,305,73)
    print('PASS original retail 68000 title bindings + original 320-wide RGB16 buffers')
    print('NOTE OP logo vertical source apertures are 110/82 rows; full cycle-exact OP still pending')

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',type=Path,required=True)
    main(ap.parse_args().rom)
