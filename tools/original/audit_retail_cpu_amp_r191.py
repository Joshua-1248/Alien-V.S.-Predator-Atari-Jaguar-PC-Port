#!/usr/bin/env python3
"""Offline provenance check of Jaguar RDB.TEXT AMP update CPU instructions.

Requires the separately supplied original Jaguar decompilation archive or its
RDB_TEXT_frozen_exact.s. Does not need, package, or modify a retail ROM. The
frozen executable is a byte-exact reference image, *not* proof that every
higher-level C reconstruction matches. This checker verifies only named AMP
phase/snapshot instructions and the production C selector.
"""
import argparse
import hashlib
from pathlib import Path
import re
import zipfile

SHA = 'c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53'
ORIGIN=0x8000
CHECKS={
  0x154cc: '067900010002a6a8',       # ADDI.W #1, ($2A6A8).L
  0x154d4: '027900070002a6a8',       # ANDI.W #7, ($2A6A8).L
  0x15524: '4a790002a6a8',           # TST.W ($2A6A8).L
  0x1552a: '6700000a',               # BEQ.W +$000A skips the snapshot
  0x1552e: '3e280026',               # MOVE.W $26(A0), D7
  0x15532: '31470028',               # MOVE.W D7, $28(A0)
}

def parse_image(text):
    tokens=re.findall(r'(?mi)^\s*dc\.w\s+\$([0-9a-f]{4})\b',text)
    data=b''.join(int(x,16).to_bytes(2,'big') for x in tokens)
    if len(data)!=99072:
        raise ValueError(f'Unexpected RDB.TEXT length {len(data)} (expected 99072)')
    digest=hashlib.sha256(data).hexdigest()
    if digest!=SHA: raise ValueError('Original CPU executable checksum mismatch: '+digest)
    return data

def load_reference(path):
    if path.suffix.lower()=='.zip':
        with zipfile.ZipFile(path) as z:
            names=[n for n in z.namelist() if n.endswith('/src/frozen_exact/RDB_TEXT_frozen_exact.s')]
            if len(names)!=1: raise ValueError('Missing or ambiguous RDB_TEXT_frozen_exact.s')
            return z.read(names[0]).decode('utf-8')
    return path.read_text(encoding='utf-8')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reference',required=True,type=Path,help='Original .zip or RDB_TEXT_frozen_exact.s')
    ap.add_argument('--port-source',default=Path(__file__).resolve().parents[2]/'src/game/amp.c',type=Path)
    args=ap.parse_args()
    data=parse_image(load_reference(args.reference))
    for address,opcode in CHECKS.items():
        expect=bytes.fromhex(opcode)
        start=address-ORIGIN
        got=data[start:start+len(expect)]
        if got!=expect: raise AssertionError(f'68000 instruction mismatch at ${address:05X}: {got.hex()} != {opcode}')
    # BEQ.W disp +$000A branches to $15538, past both copy instructions.
    disp=int.from_bytes(data[0x1552c-ORIGIN:0x1552e-ORIGIN],'big',signed=True)
    assert 0x1552e+disp==0x15538
    c=args.port_source.read_text()
    match=re.search(r'void\s+UpdateAMPs\s*\([^)]*\)\s*\{',c)
    if not match: raise AssertionError('UpdateAMPs missing')
    body=c[match.end():c.find('\n}',match.end())]
    if not re.search(r'if\s*\(entime\s*!=\s*0\s*\)\s*a->oldenergy\s*=\s*a->energy',body):
        raise AssertionError('Port oldenergy phase selector no longer follows retail 68000 BEQ')
    print('PASS: 99072-byte retail CPU image SHA-256, six AMP instruction windows,')
    print('      branch target $15538 and production phase-1..7 health snapshot selector')

if __name__=='__main__': main()
