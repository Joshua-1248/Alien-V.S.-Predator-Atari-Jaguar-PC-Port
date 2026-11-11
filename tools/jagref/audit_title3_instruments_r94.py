#!/usr/bin/env python3
"""R94 read-only original-Jaguar TITLE3 score instrument census.

Uses a caller-owned retail AVP.jag; prints only note counts and original
instrument metadata, never copies sample/music/ROM bytes to disk. This is
not an audible-volume or physical-JERRY parity claim.
"""
import argparse
import collections
import hashlib
import struct
import zlib
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--rom', required=True, type=Path)
a = p.parse_args()
rom = a.rom.read_bytes()
assert len(rom) == 4 * 1024 * 1024
assert hashlib.sha256(rom).hexdigest() == 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
rdb = zlib.decompress(rom[0xE840:], 31)
base = 0x20300

def u32(buf, off):
    return struct.unpack_from('>I', buf, off)[0]

module_addr = 0x20918
at = module_addr-base
score_ref, patch_base = struct.unpack_from('>II', rdb, at)
div, pre = struct.unpack_from('>HH', rdb, at+16)
score_loc = score_ref & 0x7FFFFF
if score_ref & 0x80000000:
    score_loc = u32(rom, score_loc) - 0x800000
else:
    score_loc = score_ref - base
counts = collections.Counter()
first = {}
for i in range(0, 0x40000, 8):
    when, ev = struct.unpack_from('>II', rom, score_loc + i)
    if when == 0x7FFFFFFF:
        break
    if not ev & 0x80000000:
        continue
    patch = (ev >> 21) & 31
    counts[patch] += 1
    first.setdefault(patch, when)
print(f'TITLE3 descriptor: ${module_addr:X}; patches ${patch_base:X}; sequencer {26590906/((div+1)*(pre+1)):.3f} ticks/s')
print('patch,notes,type,original_volume_word,first_trigger_s')
for patch in sorted(counts):
    o = patch_base-base+patch*80
    typ = u32(rdb,o)
    vol = u32(rdb, o+(24 if typ == 4 else 4))
    secs = first[patch]*(div+1)*(pre+1)/26590906
    print(f'{patch},{counts[patch]},{typ},0x{vol:08X},{secs:.3f}')
print('These are score/patch values, NOT independently verified Jaguar output gains.')
