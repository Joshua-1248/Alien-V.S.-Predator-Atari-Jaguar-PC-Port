#!/usr/bin/env python3
"""Offline source-geometry audit of retail Jaguar AVP FILES.DAT.

Reads only the user-supplied canonical AVP.jag. Reports tag/width/height,
never extracts or bundles copyrighted artwork, ROM code or image payloads.
The asset's JPEG resolution is NOT the Jaguar GPU intermediate texture layout
or the Object Processor visible rectangle. Do not infer scale from this audit.
"""
import argparse
import hashlib
from pathlib import Path

EXPECTED_SHA256 = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
FILES_OFFSET = 0x2F8178
EXPECTED = {
    205: (96, 40, 'Alien source JPEG'),
    207: (320, 80, 'Predator source JPEG'),
    208: (320, 280, 'Title background JPEG'),
    211: (192, 144, 'Attract still JPEG'),
    212: (192, 144, 'Attract still JPEG'),
    219: (192, 144, 'Attract still JPEG'),
    220: (192, 144, 'Attract still JPEG'),
    224: (192, 144, 'Attract still JPEG'),
    225: (192, 144, 'Attract still JPEG'),
    230: (192, 144, 'Attract still JPEG'),
}

def audit(path: Path):
    rom = path.read_bytes()
    digest = hashlib.sha256(rom).hexdigest()
    if len(rom) != 4194304 or digest != EXPECTED_SHA256:
        raise ValueError('Retail ROM size/SHA-256 does not match canonical AVP.jag')
    off = FILES_OFFSET
    entries = {}
    for index in range(240):
        if off+8 > len(rom):
            raise ValueError(f'Truncated FILES index {index}')
        tag = rom[off:off+4]
        size = int.from_bytes(rom[off+4:off+8], 'big')
        payload = off+8
        end = payload+size
        if end > len(rom):
            raise ValueError(f'FILES index {index} outside ROM bounds')
        if index in EXPECTED:
            if tag[:2] != b'JP' or size < 8:
                raise ValueError(f'Unexpected FILES {index} tag/size {tag!r}/{size}')
            header = rom[payload:payload+8]
            version = int.from_bytes(header[:2], 'big')
            factor = int.from_bytes(header[2:4], 'big')
            width = int.from_bytes(header[4:6], 'big')*8
            height = int.from_bytes(header[6:8], 'big')*8
            want_w,want_h,desc = EXPECTED[index]
            if version != 1 or factor not in (1,2) or (width,height)!=(want_w,want_h):
                raise ValueError(f'FILES {index}: {version=} {factor=} {width=} {height=} not expected')
            entries[index] = (width,height,desc)
        off = (end+3)&~3
    if len(entries) != len(EXPECTED):
        raise ValueError(f'Only found {len(entries)} expected frontend assets')
    print(f'Retail SHA-256 MATCH: {digest}')
    print('Asset | Decoded JPEG source | Description')
    for index, (w,h,desc) in sorted(entries.items()):
        print(f'{index:5d} | {w:3d}x{h:<3d}              | {desc}')
    print('PASS: source-asset header dimensions; OP buffer/layout not inferred')

if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom', type=Path, required=True, help='Path to your retail AVP.jag')
    args=ap.parse_args()
    audit(args.rom)
