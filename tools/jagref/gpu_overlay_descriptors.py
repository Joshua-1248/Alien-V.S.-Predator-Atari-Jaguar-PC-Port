#!/usr/bin/env python3
"""RE24/S14: audit ORIGINAL 68000 overlay fixup descriptors in a RAM capture.

This is deliberately read-only and does not insert values into the native port.
Given the GPU resident memory and 2 MiB Jaguar DRAM captured at the SAME
instant by gpu_pipeline_check.c (DUMP=1), find descriptor structures matching
MAZE/GOVERS.S's verified 68000 ED22/ED52 layout, and prove that each source
variable equals the MOVEI operand that actually reached the overlay image.

Descriptor (68k big-endian, base D):
  D+00 image begin; D+04 image end; D+08 DRAM destination;
  D+12 GPU PC of sequencer MOVEI length instruction;
  D+16 32-bit pointer to an external list of
       { GPU PC of overlay MOVEI, DRAM address of source variable } pairs;
       32-bit negative GPU PC terminates the pointed-to list.

The 68000 SWAPs source values before writing the MOVEI's low-first operand;
we check the decoded MOVEI immediate, not a raw big-endian 32-bit word.
This tool does NOT assume that a descriptor is present in captured DRAM: if
none is found it fails clearly rather than inventing a patch map.

usage: gpu_overlay_descriptors.py view_gpu.bin view_dram.bin [--json FILE]
"""
import argparse
import json
import os
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gpu_pipeline_recomp import BASE, RESIDENT_END, sequencer

DRAM_SIZE = 2 * 1024 * 1024
GPU_OVERLAY = RESIDENT_END
GPU_LOCAL_END = BASE + 4096
MAX_FIXUPS = 256


def be32(buf, addr):
    if addr < 0 or addr + 4 > len(buf):
        raise ValueError(f'out-of-bounds Jaguar longword ${addr:06X}')
    return struct.unpack_from('>I', buf, addr)[0]


def be16(buf, addr):
    if addr < 0 or addr + 2 > len(buf):
        raise ValueError(f'out-of-bounds Jaguar word ${addr:06X}')
    return struct.unpack_from('>H', buf, addr)[0]


def movei_imm(buf, addr):
    """Decode low-16-first JRISC MOVEI at a byte offset inside captured RAM."""
    if be16(buf, addr) >> 10 != 38:
        raise ValueError(f'not a JRISC MOVEI at ${addr:06X}')
    return be16(buf, addr + 2) | be16(buf, addr + 4) << 16


def parse_descriptor(dram, desc, stage, strict=True):
    """Return source-verified fixups, or raise on any ambiguous/mismatched data."""
    pc, tab, src, nlong = stage
    if desc & 3:
        raise ValueError('unaligned descriptor')
    image_beg, image_end = be32(dram, desc), be32(dram, desc + 4)
    dest, length_pc = be32(dram, desc + 8), be32(dram, desc + 12)
    if dest != src or length_pc != pc + 8:
        raise ValueError('overlay destination or sequencer length PC differs')
    size = nlong * 4
    if src + size > len(dram):
        raise ValueError('overlay lies outside DRAM')
    if not size or size > GPU_LOCAL_END - GPU_OVERLAY:
        raise ValueError('overlay exceeds GPU local memory')
    if image_end <= image_beg or image_end - image_beg > size + 8:
        raise ValueError('source image bounds inconsistent with overlay length')
    entries = []
    seen = set()
    # Original 68000 at $ED5A uses MOVEA.L 16(A4),A0 (206C 0010),
    # NOT LEA 16(A4),A0: the descriptor stores an external list pointer.
    cursor = be32(dram, desc + 16)
    if cursor & 3 or cursor + 4 > len(dram):
        raise ValueError("invalid descriptor patch-list pointer")
    for _ in range(MAX_FIXUPS):
        target_pc = be32(dram, cursor)
        cursor += 4
        if target_pc & 0x80000000:
            return {'descriptor': desc, 'entry': pc, 'table': tab,
                    'image_begin': image_beg, 'image_end': image_end,
                    'destination': src, 'longwords': nlong, 'fixups': entries}
        variable = be32(dram, cursor)
        cursor += 4
        if target_pc in seen or target_pc & 1:
            raise ValueError('duplicate or unaligned GPU patch PC')
        seen.add(target_pc)
        if not GPU_OVERLAY <= target_pc < GPU_OVERLAY + size - 5:
            raise ValueError(f'fixup target ${target_pc:06X} outside overlay')
        if variable + 4 > len(dram) or variable & 1:
            raise ValueError(f'variable ${variable:06X} unavailable in DRAM')
        overlay_offset = src + target_pc - GPU_OVERLAY
        actual = movei_imm(dram, overlay_offset)
        source_value = be32(dram, variable)
        if strict and source_value != actual:
            raise ValueError(f'GPU ${target_pc:06X} MOVEI=${actual:08X} != '
                             f'RAM variable ${variable:06X}=${source_value:08X}')
        entries.append({'gpu_pc': target_pc, 'source_variable': variable,
                        'movei_immediate': actual,
                        'variable_value': source_value, 'matches': actual == source_value})
    raise ValueError(f'descriptor ${desc:06X} lacks a bounded negative terminator')


def discover(gpu, dram, scan_start=0, scan_end=None):
    if len(gpu) != 4096 or len(dram) != DRAM_SIZE:
        raise ValueError('expect 4 KiB GPU local RAM and 2 MiB Jaguar DRAM')
    stages = sequencer(gpu)
    if not stages:
        raise ValueError('GPU resident image has no recognized sequencer stages')
    # A descriptor's +8/+12 combination fingerprints its overlay and entry.
    # Require a unique, fully validated candidate; no heuristic tie-breaking.
    index = {(src, pc + 8): (pc, tab, src, ln)
             for pc, tab, ln in stages
             for src in [be32(dram, tab) & 0x1fffff]}
    found = {pc: [] for pc, _, _ in stages}
    hi = min(len(dram) - 20, scan_end if scan_end is not None else len(dram) - 20)
    for d in range(max(0, scan_start) + ((-scan_start) % 4), hi, 4):
        key = (be32(dram, d + 8), be32(dram, d + 12))
        stage = index.get(key)
        if stage is None:
            continue
        try:
            match = parse_descriptor(dram, d, stage)
        except ValueError:
            continue
        found[stage[0]].append(match)
    results = []
    for pc, tab, ln in stages:
        matches = found[pc]
        if len(matches) > 1:
            raise ValueError(f'ambiguous descriptors for GPU sequencer ${pc:06X}: '
                             + ', '.join(f'${m["descriptor"]:06X}' for m in matches))
        if len(matches) == 1:
            results.append(matches[0])
        else:
            results.append({'entry': pc, 'table': tab,
                            'destination': be32(dram, tab) & 0x1fffff,
                            'longwords': ln, 'descriptor': None, 'fixups': [],
                            'status': 'descriptor not found in DRAM; not inferred'})
    return results


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('gpu', help='same-instant view_gpu.bin')
    p.add_argument('dram', help='same-instant view_dram.bin')
    p.add_argument('--json', metavar='FILE', help='write machine-readable map')
    args = p.parse_args(argv)
    try:
        with open(args.gpu, 'rb') as f: gpu = f.read()
        with open(args.dram, 'rb') as f: dram = f.read()
        result = discover(gpu, dram)
    except (OSError, ValueError) as e:
        p.exit(2, f'overlay descriptor audit failed: {e}\n')
    total = 0
    missing = 0
    for row in result:
        d = row['descriptor']
        if d is None:
            missing += 1
            print(f'${row["entry"]:06X}: NO VERIFIED DRAM DESCRIPTOR')
        else:
            print(f'${row["entry"]:06X}: descriptor ${d:06X}, '
                  f'overlay ${row["destination"]:06X}, {len(row["fixups"])} checked fixups')
            for fix in row['fixups']:
                total += 1
                print('  GPU ${gpu_pc:06X} <= RAM ${source_variable:06X} '
                      'value ${movei_immediate:08X} MATCH'.format(**fix))
    print(f'{total} verified fixups; {missing} unlocated descriptor(s)')
    if args.json:
        with open(args.json, 'w', encoding='utf8') as f:
            json.dump({'format': 'AVP-RE24-S14-verified-overlay-fixups-v1',
                       'stages': result}, f, indent=2, sort_keys=True)
            f.write('\n')
    return 0 if not missing else 3


if __name__ == '__main__':
    sys.exit(main())
