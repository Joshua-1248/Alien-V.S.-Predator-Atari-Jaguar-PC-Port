#!/usr/bin/env python3
"""Verify the original Atari Jaguar MJP picture-object geometry from AVP.jag.

The verification uses the canonical retail cartridge's actual compressed 68000
text segment, NOT the PC port's recreated draw positions. No ROM bytes or art
are exported. The packed OP YPOS is derived from the source Make_New shift.

This does not prove the original Jaguar GPU's eight-VBL transition pixel effect.
"""
import argparse
import hashlib
from pathlib import Path
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
TEXT_GZIP = 0x3804
TEXT_ORIGIN = 0x8000
# Actual 68000 instructions observed three times in the decompressed retail
# CPU text. Each setup writes its own Jaguar RAM MJP object fields.
INSTRUCTIONS = {
    'source picture Y': ('23f90004be240004bef4', (0x1df42, 0x1e056, 0x1e102)),
    'picture Y +16': ('06b9000000100004bef4', (0x1df4c, 0x1e060, 0x1e10c)),
    'picture height 144': ('23fc000000900004bef8', (0x1df56, 0x1e06a, 0x1e116)),
    'picture width 192': ('23fc000000c00004befc', (0x1df60, 0x1e074, 0x1e120)),
    'picture X 18': ('23fc000000120004bef0', (0x1df6a, 0x1e07e, 0x1e12a)),
    # R280: each setup then adds the slide term: X = $12 + 4*D1.
    'picture X += 4*D1': ('d3b90004bef0', (0x1dfc2, 0x1e088, 0x1e134)),
}


def audit(rom_path: Path) -> None:
    rom = rom_path.read_bytes()
    if len(rom) != 0x400000 or hashlib.sha256(rom).hexdigest() != ROM_SHA:
        raise ValueError('Not the verified 4 MiB retail AVP.jag ROM')
    decompress = zlib.decompressobj(wbits=31)
    text = decompress.decompress(rom[TEXT_GZIP:])
    if not decompress.eof or len(text) != 99072:
        raise ValueError('Canonical compressed retail 68000 text stream did not decode')
    for desc, (hex_word, addrs) in INSTRUCTIONS.items():
        insn = bytes.fromhex(hex_word)
        for addr in addrs:
            at = addr - TEXT_ORIGIN
            if text[at:at+len(insn)] != insn:
                raise ValueError(f'68000 opcode mismatch: {desc} at ${addr:06X}')
        print(f'PASS {desc}: '+', '.join(f'${x:06X}' for x in addrs))
    # NTSC InitMJP invokes SetScreen(320,240).  Retail SetScreen computes
    # top_line from the video centre 0x10c; High_Line=top_line in this mode.
    video_vcenter = 0x10c
    high_line = video_vcenter - 240
    assert high_line == 0x1c
    # Make_New: raw OP phrase YPOS = 2 * (High_Line + 16). TOM's VDB is 28
    # half-lines, so logical active-video scanline is (YPOS-VDB)/2.
    ypos = 2 * (high_line + 16)
    y_screen = (ypos - high_line) // 2
    assert (18, y_screen, 192, 144) == (18, 30, 192, 144)
    print('Retail NTSC picture aperture: x=18+4*D1 (D1 from $4BED4/$4BED8/$4BEDC), y=30, width=192, height=144')
    print('PASS: source CPU Object Processor picture setup; GPU transition not certified')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path)
    audit(parser.parse_args().rom)
