#!/usr/bin/env python3
"""Cross-check retail title OP scanline origins against a ROM-running Jaguar model.

    python tools/audit_retail_title_op_origin_r81.py --rom PRIVATE/AVP.jag \
        --jagref BUILD/jagref

Uses the owner's exact retail ROM as private input. Starts the project's Jaguar
reference model for two original fields, checks the OP phrase values and
reports only numeric metadata. No retail images, ROM data, or SDK files are
written to any public release. This is a MODEL comparison, not proof of
hardware-cycle-perfect video output on a physical Jaguar.
"""
from __future__ import annotations
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile

SHA256 = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
# Jaguar model OP fields, verified with source CPU MJP. Frame 330: ALIEN
# scale=37 (GPU phase7), POS_Y=92, title background YPOS=28; field 506:
# PREDATOR phase31 YPOS=105, scale17; title backdrop remains YPOS=28.
EXPECTED = {
    330: {0x050C00:(0,28,0),0x050C40:(1,92,37)},
    506: {0x050C00:(0,28,0),0x050C80:(1,105,17)},
}
PAT = re.compile(r'obj@([0-9a-fA-F]{6})\s+\S+\s+type=(\d+)\s+y=(\d+)(.*)')

def verify_model(binary:Path,rom:Path,field:int) -> None:
    env=os.environ.copy()
    env['OPLIST']='108'
    with tempfile.TemporaryDirectory(prefix='avp_title_op_') as temp:
        p=subprocess.run([str(binary),str(rom),str(field),str(field),temp],
                         text=True,capture_output=True,env=env,timeout=45,check=True)
    actual={}
    for row in p.stdout.splitlines():
        m=PAT.search(row)
        if m:
            addr=int(m.group(1),16);kind=int(m.group(2));y=int(m.group(3))
            scale=re.search(r'\bhs=(\d+)',m.group(4))
            actual[addr]=(kind,y,int(scale.group(1)) if scale else 0)
    for addr,expected in EXPECTED[field].items():
        if actual.get(addr)!=expected:
            raise AssertionError(f'field {field} OP {addr:06X}: '
                                 f'expected {expected}, found {actual.get(addr)}')
    print(f'PASS model field {field}: '+', '.join(
        f'${addr:06X} YPOS={row[1]}, HSCALE={row[2]}'
        for addr,row in EXPECTED[field].items()))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',type=Path,required=True)
    ap.add_argument('--jagref',type=Path,required=True)
    args=ap.parse_args()
    rom=args.rom.resolve()
    binary=args.jagref.resolve()
    if not binary.is_file():raise ValueError('Jaguar reference-machine executable not found')
    if rom.stat().st_size!=4194304 or hashlib.sha256(rom.read_bytes()).hexdigest()!=SHA256:
        raise ValueError('ROM does not match verified retail AVP.jag')
    for field in EXPECTED:verify_model(binary,rom,field)
    print('PASS original NTSC title YPOS uses Zero_Line=VDB/2, not VDB')
    print('LIMIT: model is not a physical-hardware scanline/cycle certificate')

if __name__=='__main__':main()
