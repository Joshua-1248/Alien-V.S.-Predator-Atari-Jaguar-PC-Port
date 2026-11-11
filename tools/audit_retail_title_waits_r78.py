#!/usr/bin/env python3
"""Assert cartridge-authored title waits without exporting any ROM bytes."""
from pathlib import Path
import argparse,hashlib,zlib
ROM_SHA='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
CPU_SHA='c8c8de5b3723c56070ec9b6195e8d191101071c3d9e2b9de90b9bd400cdf4b53'
ORIGIN=0x8000
sites={
 0x01F118:'4eb90001fa3a',        # native title call
 0x01FC08:'23fc0000001e0004bf2c',  # authored 30-field wait counter
 0x01FC12:'4eb90001f05e',        # wait routine
 0x01FC98:'23fc000001040004bf2c', # authored 260-field wait counter
 0x01FCA2:'4eb90001f05e',        # same wait routine
 0x01F06E:'4eb90001e15a',        # One_Tick inside wait
 0x01F07E:'66de',                # branch back to wait loop (validate below)
}
def main(path):
 rom=Path(path).read_bytes()
 assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA,'Retail ROM mismatch'
 decomp=zlib.decompressobj(31);cpu=decomp.decompress(rom[0x3804:])
 assert decomp.eof and len(cpu)==99072 and hashlib.sha256(cpu).hexdigest()==CPU_SHA
 for addr,hx in sites.items():
  s=cpu[addr-ORIGIN:addr-ORIGIN+len(bytes.fromhex(hx))]
  if s!=bytes.fromhex(hx):raise AssertionError(f'Code mismatch at {addr:06x}: got {s.hex()}')
 print('PASS original title entry, 30- and 260-VBlank wait counters, wait calls, One_Tick')
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True);a=ap.parse_args();main(a.rom)
