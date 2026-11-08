#!/usr/bin/env python3
"""RE #21 R213: offline retail-ROM title SCBITOBJ scale instruction audit.

Requires the owner's untouched AVP.jag. Validates CPU instruction sites and
calculates title vertical scanline counts from the Jaguar OP 3.5 remainder
algorithm. Does not extract or emit proprietary ROM code/pixels and never runs
inside the game.
"""
import argparse
import hashlib
from pathlib import Path
import zlib

ROM_SHA256 = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
# Original executable is mapped at $008000; these CPU instruction locations
# correspond to the title Update2 and Update routines audited in R205/R212.
SITES = (0x1E752, 0x1E7EE)
# ADD.L #$20,D2; two LSL.L shifts; OR.L D2,D1 -- original 68000 phrase
# REMAINDER composition. The sequence is checked by ROM-relative location,
# not used as replacement code.
PHRASE_REMAINDER_PATTERN = bytes.fromhex('d4bc00000020e18ae18a8282')


def scan_rows(source_height: int, vscale: int):
    """SCBITOBJ: output data pointer and live remainder at each line."""
    remainder = vscale + 0x20
    src = 0
    rows = []
    while src < source_height:
        rows.append(src)
        # OP updates the next source row AFTER the current displayed line.
        while remainder < 0x20:
            remainder += vscale
            src += 1
        remainder -= 0x20
    return rows


def audit(rom_path: Path):
    rom = rom_path.read_bytes()
    if len(rom) != 4_194_304 or hashlib.sha256(rom).hexdigest() != ROM_SHA256:
        raise ValueError('Expected the exact 4 MiB AVP Jaguar retail ROM')
    stream = zlib.decompressobj(wbits=31)
    cpu = stream.decompress(rom[0x3804:])
    if not stream.eof or len(cpu) != 99_072:
        raise ValueError('Incorrect original 68000 executable stream')
    for address in SITES:
        if cpu[address - 0x8000:address - 0x8000 + len(PHRASE_REMAINDER_PATTERN)] != PHRASE_REMAINDER_PATTERN:
            raise ValueError(f'Original title object scale phrase opcode differs at ${address:06X}')
    print('PASS: original retail 68000 Update2 and Update +$20 scaling phrase sites')
    for label, height, scales in (('ALIEN', 110, range(12, 45)), ('PREDATOR', 82, range(16, 33))):
        for scale in scales:
            rows = scan_rows(height, scale)
            expected = (height * scale) // 0x20 + 2
            if len(rows) != expected or any(a > b for a, b in zip(rows, rows[1:])) or rows[-1] >= height:
                raise ValueError(f'{label}: invalid scanline sequence at scale {scale}')
        print(f'PASS: {label} original OP vertical phases {min(scales)}..{max(scales)}, source aperture {height}')
    rows = scan_rows(110, 12)
    if rows[:7] != [0, 0, 2, 5, 7, 10, 13] or rows[-1] != 109 or len(rows) != 43:
        raise ValueError('Retail final ALIEN scale-12 scanline order mismatch')
    print('PASS: ALIEN final scale 12: 43 lines, source rows 0,0,2,5,7,10,13,...,109')
    print('NOT VERIFIED: physical-Jaguar pixel captures, OP list timing, RGB buffer overread, GPU page transition')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom', type=Path, required=True)
    audit(ap.parse_args().rom)
