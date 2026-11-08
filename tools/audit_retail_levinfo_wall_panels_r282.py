#!/usr/bin/env python3
"""R282: port wall-panel lists (src/jaguar/avp_gpu/panels.cpp) == retail LEVINFO.S
l*_panels wall bytes in the user's AVP.jag RDB data.  Read-only; prints no ROM data.
"""
import argparse, hashlib, pathlib, re, zlib
ROM_SHA='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
NAMES={'SUBLEVEL':0x80,'ARMOURY':0x81,'MEDLAB':0x82,'DOCKING':0x83,'CANTEEN':0x84,'ELEVATOR':0x85,
       'AIRNOTICE':0x86,'PETTYOFF':0x87,'COMMAND':0x88,'OFFAREA':0x89,'TRAINING':0x8a,'BARRACKS':0x8b,'BRIG':0x8c}
# RDB data addresses of each wall list (retail image, $8000 base), terminated by $FF.
ADDR={'l15w':0x240A8,'l2w':0x24130,'l3w':0x241BA,'l4w':0x2424C,'ductw':0x243AE,'predw':0x243DA,'alienw':0x24428}
def image(rom):
    # RDB data follows the 68000 text in the canonical second GZIP stream at 0xE840.
    d=zlib.decompressobj(wbits=31); text=d.decompress(rom[0x3804:])
    d2=zlib.decompressobj(wbits=31); data=d2.decompress(rom[0xE840:])
    return text,data
def main(rom_path,source):
    rom=rom_path.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA
    text,data=image(rom)
    # data stream begins at $20300 (retail memory map).
    src=(source/'src/jaguar/avp_gpu/panels.cpp').read_text()
    for name,addr in ADDR.items():
        m=re.search(r'A\(%s,([^)]*)\)'%name,src); assert m,name
        vals=[NAMES[t.strip()] if t.strip() in NAMES else int(t.strip()) for t in m.group(1).split(',')]
        off=addr-0x20300
        rombytes=list(data[off:off+len(vals)+1])
        assert rombytes[:-1]==vals and rombytes[-1]==0xFF,(name,hex(addr))
        print(f'PASS {name} == retail RDB ${addr:06X} ({len(vals)} wall slots)')
    print('PASS all port wall-panel lists equal the retail LEVINFO.S bytes')
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',required=True,type=pathlib.Path)
    ap.add_argument('--source',default=pathlib.Path('.'),type=pathlib.Path)
    a=ap.parse_args();main(a.rom,a.source)
