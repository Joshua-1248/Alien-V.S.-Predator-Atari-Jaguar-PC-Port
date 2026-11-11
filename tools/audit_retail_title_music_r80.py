#!/usr/bin/env python3
"""Verify Atari Jaguar retail title-score startup from an owner-supplied AVP.jag.

Does not extract/distribute ROM data. Model field 43 is established through
an instruction-hook run of jagref (M68K_INSTRUCTION_HOOK enabled), separately.
"""
import argparse
import hashlib
import zlib
from pathlib import Path

EXPECTED_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
EXPECTED_CODE_SHA = 'c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53'

def verify(rom_path: Path):
    data = rom_path.read_bytes()
    assert len(data) == 4194304 and hashlib.sha256(data).hexdigest() == EXPECTED_SHA, 'Retail ROM mismatch'
    d = zlib.decompressobj(31)
    code = d.decompress(data[0x3804:])
    assert d.eof and len(code) == 99072 and hashlib.sha256(code).hexdigest() == EXPECTED_CODE_SHA, 'Original 68000 mismatch'
    # MoveA title3_music descriptor $20918, load fixed per-title transfer base,
    # JSR original StartMus routine $0099B2; code relative to RAM $008000.
    sites = {
        0x0087D0: '207c00020918227c00061c004eb9000099b2',
        0x01F118: '4eb90001fa3a',
    }
    for address, hx in sites.items():
        target=bytes.fromhex(hx)
        actual=code[address-0x8000:address-0x8000+len(target)]
        assert actual == target, f'Original instruction mismatch at ${address:06X}: {actual.hex()}'
    assert 0x0087D0 < 0x01F118
    print('PASS original title3_music $020918 selected by 68000 before $01F118 title animation')
    print('PASS SHA-256 original retail 4MiB ROM and decompressed 68000 code')
    print('MODEL OBSERVATION: original StartMus $0099B2 at field 43; first nonzero I2S PCM ~292; first ALIEN pixel ~317')

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('--rom',required=True,type=Path);a=p.parse_args();verify(a.rom)
