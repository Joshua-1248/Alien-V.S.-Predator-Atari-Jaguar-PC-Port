#!/usr/bin/env python3
"""ROM-only MJP Show_Tex sound-gate proof; emits no retail executable/data."""
from pathlib import Path
import argparse,hashlib,zlib
ROM_SHA='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
# Fixed original 68000 instructions at their original RAM PCs.
# All values independently checked against retail AVP.jag GZIP MAIN image.
CODE={
    0x01D2FE:'b0bc00000020',     # CMP.L #$20,D0 (space)
    0x01D304:'66000004',         # BNE; else MOVEQ #$5F,D0
    0x01D308:'705f',             # space converts to underscore
    0x01D30A:'b0bc0000005f',     # underscore skips sound
    0x01D310:'67000066',         # BEQ after sound section
    0x01D314:'b0bc00000000',     # NUL skips sound
    0x01D31A:'6700005c',
    0x01D31E:'0cb8000000017fac', # SFX mode #1: selection
    0x01D32A:'0cb8000000027fac', # SFX mode #2: introduction
    0x01D336:'0cb8000000047fac', # SFX mode #4: escape
    0x01D342:'0cb8000000077fac', # SFX mode #7: termination
    0x01D352:'0cb9000000010004bec8', # no TFX when fast flag ==1
    0x01D360:'48e7f8f8',         # preserve 68000 registers
    0x01D364:'74ff72ff',         # D2=-1, D1=-1
    0x01D368:'203c000221b8',     # typewriter FullSynth descriptor
    0x01D36E:'4eb900009c6c',     # JSR MUSIC.o TFX, not direct host PCM
    0x01D374:'4cdf1f1f',         # restore
    0x01D378:'b0bc0000007e',     # tilde processing comes AFTER TFX
}
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rom',required=True,type=Path)
a=parser.parse_args()
r=a.rom.read_bytes()
if len(r)!=4194304 or hashlib.sha256(r).hexdigest()!=ROM_SHA: raise SystemExit('ERROR: not canonical AVP.jag')
c=zlib.decompressobj(31).decompress(r[0x3804:])
if len(c)!=99072: raise SystemExit('ERROR: retail CPU image length')
for address,h in CODE.items():
    b=bytes.fromhex(h);got=c[address-0x8000:address-0x8000+len(b)]
    if got!=b: raise SystemExit(f'ERROR: mismatch at ${address:06X}: {got.hex()} != {b.hex()}')
print(f'PASS {len(CODE)} ROM-exact Show_Tex TFX opcode groups; silent space/underscore, modes 1/2/4/7, TFX $9C6C')
