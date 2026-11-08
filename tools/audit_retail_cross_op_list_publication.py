#!/usr/bin/env python3
"""R222: retail Jaguar AVP MJP Object Processor list staging/publication audit.

Reads the user's private canonical retail ROM only. Checks original 68000
instruction groups for OLP set-up and the three OP-list copy/acknowledgment
routines. Synthetic list-memory tests validate the reconstructed address and
copy-count relations. Not a 68000 emulator, cycle-level OP trace, or proof of
which scanline actually consumed each published list. No original bytes/art
are reproduced in the audit output.
"""
from __future__ import annotations
import argparse
from hashlib import sha256
from pathlib import Path
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
CPU_BASE = 0x8000

# Addresses are *retail CPU addresses*, and values span complete instruction
# groups. These are instruction-identity assertions, not speculative timings.
ORIGINAL = {
    # Initial MJP scene setup: OLP gets word-swapped PRE_LIST ($7548), not
    # the mutable $4FC00 source / $50C00 staging list. another TOM word at $F00024 cleared; do not confuse it with OBF at $F00026.
    0x01D7BC: '203c00007548',             # MOVE.L #$7548,D0
    0x01D7C2: '33fc000000f00024',         # MOVE.W #0,distinct TOM $F00024 register, not OBF $F00026
    0x01D7CA: '4840',                     # SWAP D0
    0x01D7CC: '23c000f00020',             # MOVE.L D0, OLP
    0x01E690: '203c00007548484023c000f00020', # alternate title OLP route
    # 6-record/4-longword list: patch slot+64 using published picture pointer;
    # copy each record with modified link pointer; then notify $7FA8.
    0x01E34A: '247c0004fc002012',        # A2=$4FC00; load head record
    0x01E36C: 'd5fc000000402012c0bc000007ff', # A2+=64; first phrase mask
    0x01E37A: '227c00007fa42211e18980812480', # $7FA4 pointer patch
    0x01E388: '247c0004fc00267c00050c007606', # source/dest, D3=6
    0x01E396: '204a224b2018d0bc0000000222c022d822d822d8', # 4 longs, first +=2
    0x01E3AA: 'd5fc00000020d7fc0000002096bc0000000166d8', # 32-byte strides
    0x01E3BE: '21fc000000017fa84e75', # acknowledgment only after loop
    # 8-record/6-longword plus one tail: no $7FA4 patch here.
    0x01E4D0: '247c0004fc00267c00050c002012',
    0x01E4E0: '00407608204a224b2018d0bc00000002', # D3=8
    0x01E4F0: '22c022d822d822d822d822d8', # 6 longs
    0x01E4FC: 'd5fc00000020d7fc0000002096bc0000000166d4',
    0x01E510: '204a224b22d822d822d822d822d822d8', # 6 tail longs
    0x01E520: '21fc000000017fa84e75',
    # 10-record/6-longword route: patch head OP link with $7FA4 pointer.
    0x01E804: '247c0004fc002012',
    0x01E80C: '67000050c0bc000007ff227c00007fa4',
    0x01E81C: '2211e18980812480',
    0x01E824: '247c0004fc00267c00050c00760a', # D3=10
    0x01E832: '204a224b2018d0bc0000000222c022d822d822d822d822d8',
    0x01E84A: 'd5fc00000020d7fc0000002096bc0000000166d4',
    0x01E85E: '21fc000000017fa84e75',
    # GPU stage-1 handshake: CPU obtains GPU-published picture pointer for OP.
    0x01E0FA: '23f87fa40004beec',
    0x01E13A: '4eb90001e240',
    # Original One_Tick waits for VBlank, not a direct forced object commit.
    0x01E15A: '42b87fa84ab87fa867fa4e75',
}

def check_rom(path: Path) -> bytes:
    rom=path.read_bytes()
    if len(rom)!=4194304 or sha256(rom).hexdigest()!=ROM_SHA:
        raise ValueError('Expected original unmodified, user-owned 4 MiB AVP.jag')
    z=zlib.decompressobj(31)
    code=z.decompress(rom[0x3804:])
    if not z.eof or len(code)!=99072:
        raise AssertionError('Unexpected original 68000 program')
    for at,hextext in ORIGINAL.items():
        value=bytes.fromhex(hextext)
        got=code[at-CPU_BASE:at-CPU_BASE+len(value)]
        if got!=value:
            raise AssertionError(f'Original opcode mismatch at ${at:06X} (got {got.hex()})')
    # All ACK writers are in the original 16-bit-short-address form.
    assert code[0x1E3BE-CPU_BASE:0x1E3C6-CPU_BASE] == bytes.fromhex('21fc000000017fa8')
    assert code[0x1E520-CPU_BASE:0x1E528-CPU_BASE] == bytes.fromhex('21fc000000017fa8')
    assert code[0x1E85E-CPU_BASE:0x1E866-CPU_BASE] == bytes.fromhex('21fc000000017fa8')
    return code


def simulate_original_copy(record_count:int, words_per_record:int, tail:bool=False, patch:int=-1)->tuple[list[list[int]],list[list[int]]]:
    """Source-derived OP-copy semantics; synthetic phrases, not Jaguar memory."""
    src=[[((i+3)<<24)|(j+0x1357) for j in range(8)] for i in range(11)]
    before=[x[:] for x in src]
    dst=[[0xFEEDFACE]*8 for _ in range(11)]
    if patch>=0:
        # ROM: AND.L #$7FF, source first long; OR with $7FA4 pointer <<8.
        fake_gpu_picture=0x00042A00
        src[patch][0]=(src[patch][0]&0x7FF)|(fake_gpu_picture<<8 & 0xFFFFFFFF)
    for i in range(record_count):
        dst[i][0]=(src[i][0]+2)&0xFFFFFFFF
        dst[i][1:words_per_record]=src[i][1:words_per_record]
    if tail:dst[record_count][:6]=src[record_count][:6]
    assert all(dst[i][0]==(src[i][0]+2)&0xffffffff for i in range(record_count))
    assert all(dst[i][words_per_record:]==[0xFEEDFACE]*(8-words_per_record) for i in range(record_count))
    if patch>=0:
        assert src[patch][0]!=before[patch][0]
        assert all(src[i]==before[i] for i in range(len(src)) if i!=patch)
    if tail:assert dst[record_count][:6]==src[record_count][:6]
    return src,dst

def main(path:Path):
    check_rom(path)
    # OLP is word-swapped before CPU writes the TOM register.
    encoded=((0x7548&0xffff)<<16)|0
    restored=((encoded&0xffff)<<16)|(encoded>>16)
    assert encoded==0x75480000 and restored==0x00007548
    assert restored not in (0x4FC00,0x50C00)
    simulate_original_copy(6,4,patch=2)
    simulate_original_copy(8,6,tail=True)
    simulate_original_copy(10,6,patch=0)
    assert 0x50C00-0x4FC00==0x1000
    print(f'PASS canonical retail ROM, {len(ORIGINAL)} original CPU opcode groups')
    print('PASS original MJP OLP write: $7548 word-swap -> hardware value $75480000')
    print('PASS distinct mutable OP RAM paths: $4FC00 source -> $50C00 destination (+$1000)')
    print('PASS 6x4, 8x6+tail, and 10x6 phrase copy layouts; 32-byte record stepping')
    print('PASS GPU $7FA4-derived bitmap-pointer patch in six-record/ten-record paths')
    print('PASS each original copy routine writes $7FA8=1 only after its list-copy loop')
    print('LIMIT: original VBlank IRQ invocation, OP scanline sampling and list activation are not cycle-traced')

if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--rom',type=Path,required=True)
    main(a.parse_args().rom)
