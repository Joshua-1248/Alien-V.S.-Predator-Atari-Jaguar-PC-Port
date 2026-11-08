#!/usr/bin/env python3
"""Verify a user-local Prototype Texture Restoration pack against the S106 mapping.

Usage: python3 tools/verify_prototype_texture_pack.py prototype_texture_restoration
No original game ROM or texture source archives are required for verification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='Folder containing generated .prgb textures')
    args = parser.parse_args()
    pack = args.directory
    manifest = json.loads((Path(__file__).resolve().parent /
                           'prototype_texture_restoration_manifest.json').read_text())
    expected = {f"{entry['retail_file_id']:02}.prgb": entry for entry in manifest['texture_slots']
                if entry['priority'] != 'retail'}
    problems = []
    for filename, entry in sorted(expected.items()):
        path = pack / filename
        if not path.is_file():
            problems.append(f'Missing: {filename}')
            continue
        data = path.read_bytes()
        if len(data) != 49168:
            problems.append(f'Wrong length: {filename}: {len(data)} bytes')
            continue
        magic, length, checksum = struct.unpack('<8sII', data[:16])
        rgb = data[16:]
        if magic != b'AVPTEX1\0' or length != 49152:
            problems.append(f'Invalid header: {filename}')
        if zlib.crc32(rgb) != checksum:
            problems.append(f'Corrupt data: {filename}')
        if hashlib.sha256(rgb).hexdigest() != entry['source_rgb_sha256']:
            problems.append(f'Incorrect source image: {filename}')
    for path in sorted(pack.glob('*.prgb')):
        if path.name not in expected:
            problems.append(f'Unexpected numbered replacement: {path.name}')
    if problems:
        for message in problems:
            print(message, file=sys.stderr)
        print(f'FAIL: {len(problems)} issue(s)', file=sys.stderr)
        return 1
    print(f'PASS: {len(expected)} intact, correctly mapped prototype RGB panels')
    return 0


if __name__ == '__main__':
    sys.exit(main())
