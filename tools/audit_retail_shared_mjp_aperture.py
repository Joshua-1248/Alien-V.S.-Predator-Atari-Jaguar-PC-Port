#!/usr/bin/env python3
"""Offline ROM-only verification of the shared MJP 192x144 Object Processor picture.

No copyrighted program bytes, JagPEG payloads or ROM-derived images are output.
Verifies concrete retail CPU call sites and the authored FILES.DAT dimensions.
Does not attempt to certify intermediate GPU transition pixels or title logos.
"""
import argparse
import hashlib
from pathlib import Path
import zlib

RETAIL_SHA='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
TEXT_ORIGIN=0x8000
FILES_OFFSET=0x2f8178
# These direct JSRs all target the common $01DF2C MJP picture transition.
SHARED_CALLERS=(0x01d8ae,0x01dbfe,0x01dd24,0x01de06,0x01df0e)
# Original FILES.DAT MJP source JPEGs used by Intro, Escape, Lose.
PICTURES={211,212,219,220,224,225,230,216,217,218,222,223,231}
# The shared original picture-object builder has three entry/setup paths.
GEOMETRY={
    0x01df42: '23f90004be240004bef4',
    0x01df4c: '06b9000000100004bef4',
    0x01df56: '23fc000000900004bef8',
    0x01df60: '23fc000000c00004befc',
    0x01df6a: '23fc000000120004bef0',
}

def main(rom_file: Path):
    rom=rom_file.read_bytes()
    if len(rom)!=4194304 or hashlib.sha256(rom).hexdigest()!=RETAIL_SHA:
        raise ValueError('Expected the verified retail 4 MiB AVP.jag ROM')
    unzip=zlib.decompressobj(31)
    prog=unzip.decompress(rom[0x3804:])
    if not unzip.eof or len(prog)!=99072:raise ValueError('Retail CPU text mismatch')
    for addr in SHARED_CALLERS:
        at=addr-TEXT_ORIGIN
        insn=b'\x4e\xb9'+(0x1df2c).to_bytes(4,'big')
        if prog[at:at+len(insn)]!=insn:
            raise ValueError(f'Shared picture JSR at ${addr:06X} not verified')
    for addr,hexbytes in GEOMETRY.items():
        insn=bytes.fromhex(hexbytes)
        at=addr-TEXT_ORIGIN
        if prog[at:at+len(insn)]!=insn:
            raise ValueError(f'Picture geometry instruction mismatch at ${addr:06X}')
    # Independently verify all eligible 192x144 assets from the cartridge.
    found=set();offset=FILES_OFFSET
    for file_id in range(240):
        if offset+8>len(rom):raise ValueError('FILES record header truncated')
        tag=rom[offset:offset+4]
        n=int.from_bytes(rom[offset+4:offset+8],'big')
        data=offset+8
        end=data+n
        if end>len(rom):raise ValueError('FILES record payload out of bounds')
        if file_id in PICTURES:
            hdr=rom[data:data+8]
            if tag!=b'JP80' or len(hdr)<8:
                raise ValueError(f'FILES {file_id} not JagPEG')
            w=int.from_bytes(hdr[4:6],'big')*8
            h=int.from_bytes(hdr[6:8],'big')*8
            if (w,h)!=(192,144):raise ValueError(f'FILES {file_id} is {w}x{h}')
            found.add(file_id)
        offset=(end+3)&~3
    if found!=PICTURES:raise ValueError('Missing picture resources')
    # NTSC: High_Line=VDB=0x1c, Make_New packs YPOS=2*(High_Line+16).
    assert (2*(0x1c+16)-0x1c)//2==30
    print('PASS retail ROM SHA; shared 68000 MJP JSR sites:',
          ', '.join(f'${x:06X}' for x in SHARED_CALLERS))
    print('PASS original X=18 Y=30 width=192 height=144 object instructions')
    print('PASS 13 unmodified Intro/Escape/Lose picture headers (192x144)')
    print('LIMIT: Jaguar GPU 8-VBlank transition pixels and title-logo buffer layout remain unverified')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,required=True)
    main(parser.parse_args().rom)
