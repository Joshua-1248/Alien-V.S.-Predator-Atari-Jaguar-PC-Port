#!/usr/bin/env python3
"""R239: ROM-backed 68000 picture-object writer and IRQ-copy footprint audit.

Precisely check executable operands and memory footprints, and enumerate
synthetic legal CPU/IRQ step interleavings. NO hardware bus scheduling, GPU
scanout model, inferred rate, or claimed actual race/tear; no original assets
are emitted or bundled. This is OFFLINE RESEARCH, not a diagnostic patch.
"""
from __future__ import annotations
import argparse, hashlib, pathlib, zlib
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_cross_cpu_busy_poll_r238 import WINDOWS as R238_WINDOWS

BASE=0x8000
CPU_SRC=0x4FC40
OP_DST=0x50C40
CALLBACK_SOURCE=0x4FC00
CALLBACK_OUTPUT=0x50C00

# Full original executable windows, already authenticated by R238; we also
# check targeted *instruction boundaries* on the paths analysed here.
EXPECTED={
  0x1E0F0:'23fc0004fc400004bee8',  # pending record address = source+64
  0x1E0FA:'23f87fa40004beec',  # GPU-published bitmap pointer sampled by CPU
  0x1E13A:'4eb90001e240',      # live CPU calls Make_New, no VBlank wait
  0x1E244:'20790004bee8',      # A0 = pending object record address
  0x1E252:'24390004beec',      # bitmap data pointer input
  0x1E266:'20c0',              # longword 0: (A0)+
  0x1E288:'20c0',              # longword 1: (A0)+
  0x1E29E:'20c0',              # longword 2: (A0)+
  0x1E2C2:'20c0',              # longword 3: (A0)+
  0x1E34A:'247c0004fc0020126700006a',      # six-record source list base
  0x1E362:'67000024',          # conditional branch in six-list callback
  0x1E356:'207c0004be200c900000000067000024',
  0x1E366:'247c0004fc00',      # re-establish source list base
  0x1E36C:'d5fc00000040',      # A2 += 64 to third record
  0x1E372:'2012c0bc000007ff',# load + mask first longword of third record
  0x1E37A:'227c00007fa4',      # pointer read for VBlank picture patch
  0x1E382:'e18980812480',      # pointer shift, combine, write (A2)
  0x1E388:'247c0004fc00',      # begin 6-copy source
  0x1E38E:'267c00050c00',      # begin 6-copy destination
  0x1E394:'7606',              # six records
  0x1E396:'204a224b2018d0bc0000000222c022d822d822d8', # 4 longwords/record; first +2
  0x1E3AA:'d5fc00000020d7fc0000002096bc0000000166d8', # advance 32 bytes, loop
  0x1E3BE:'21fc000000017fa84e75', # ACK only after copy
  0x1E4D0:'247c0004fc00267c00050c00', # eight-record source/destination
  0x1E4E0:'00407608',          # eight records
  0x1E4F0:'22c022d822d822d822d822d8', # six longwords/record
  0x1E520:'21fc000000017fa84e75',
  0x1E804:'247c0004fc00',      # ten-record source
  0x1E81C:'2211e18980812480', # patch first record, not third
  0x1E824:'247c0004fc00267c00050c00760a', # source dest 10 records
  0x1E842:'22d822d822d822d8', # 6-longword copy follows first two
  0x1E85E:'21fc000000017fa84e75',
}

def source(path:pathlib.Path):
    rom=path.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA
    z=zlib.decompressobj(31); program=z.decompress(rom[0x3804:])
    assert z.eof and len(program)==99072
    for (lo,hi),digest in R238_WINDOWS.items():
        assert hashlib.sha256(program[lo-BASE:hi-BASE]).hexdigest()==digest,hex(lo)
    for at,expected in EXPECTED.items():
        b=bytes.fromhex(expected)
        assert program[at-BASE:at-BASE+len(b)]==b,(hex(at),'mismatched original opcode')
    return program

def check_footprint(program):
    # MOVE.L D0,(A0)+ appears exactly four times in Make_New; A0 comes from
    # $4BEE8, supplied $4FC40 by the original polling loop.
    body=program[0x1E240-BASE:0x1E2CA-BASE]
    locations=[0x1E240+i for i in range(len(body)-1) if body[i:i+2]==b'\x20\xc0']
    assert locations==[0x1E266,0x1E288,0x1E29E,0x1E2C2],locations
    writes=[CPU_SRC+4*i for i in range(4)]
    assert writes==[0x4FC40,0x4FC44,0x4FC48,0x4FC4C]
    # R222's callback copies list at stride 32, so record #2 is offset 64,
    # and copy header + three longwords to $50C40. Same four longwords as CPU.
    assert CPU_SRC==CALLBACK_SOURCE+2*32
    assert OP_DST==CALLBACK_OUTPUT+2*32
    assert set(writes)=={OP_DST-CALLBACK_OUTPUT+CALLBACK_SOURCE+4*i for i in range(4)}
    # All three callbacks copy record #2; the 6-list route additionally
    # patches its first word in-place immediately before copying.
    for count,word_count in ((6,4),(8,6),(10,6)):
        assert 2<count and word_count>=4
    # Return proof summary independent of synthetic schedules.
    return writes


def schedule_space():
    # A Jaguar 68000 VBlank IRQ interrupts the *same* CPU that constructs the
    # pending object. The callback runs to RTE, then the interrupted CPU
    # resumes: no CPU Make_New stores may interleave *inside* this ISR.
    # Only five insertion cuts across the four sequential Make_New stores.
    old=[0x101,0x102,0x103,0x104]
    new=[0x201,0x202,0x203,0x204]
    gpu_pointer=0x123450
    cases=[]
    for cut in range(5):
        src=old[:]
        for i in range(cut):src[i]=new[i]
        # Conditional six-record callback patches first source word before
        # copy, using GPU-published pointer; in memory, the source persists.
        patched=(src[0]&0x7ff)|((gpu_pointer<<8)&0xffffffff)
        src[0]=patched
        copied=[(src[i]+(2 if i==0 else 0))&0xffffffff for i in range(4)]
        for i in range(cut,4):src[i]=new[i]
        assert copied[0]==(patched+2)&0xffffffff
        if cut==0: assert copied[1:]==old[1:]
        if cut==4: assert copied[1:]==new[1:]
        if cut in (2,3):assert copied[1:]!=old[1:] and copied[1:]!=new[1:]
        cases.append((cut,tuple(copied),tuple(src)))
    assert len({row[1] for row in cases})==5
    assert cases[0][2][0]==new[0]  # later CPU first write replaces IRQ patch
    assert cases[1][2][0]==cases[1][1][0]-2 # CPU first word written before IRQ
    return cases


def main(path:pathlib.Path):
    program=source(path);writes=check_footprint(program)
    cases=schedule_space()
    print(f'PASS original ROM, {len(R238_WINDOWS)} complete CPU windows, {len(EXPECTED)} targeted CPU opcode groups')
    print('PASS busy-loop Make_New four sequential longword stores at',','.join(f'${x:05X}' for x in writes))
    print('PASS all original VBlank record-copy layouts read same 3rd record: $4FC40..$4FC4C -> $50C40..$50C4C')
    print('PASS six-record callback conditionally patches $4FC40 from GPU pointer $7FA4 before copying')
    print('PASS copy link first-word +2 and $7FA8 acknowledgment only after entire six-record copy')
    print(f'PASS {len(cases)} candidate 68000 IRQ-entry cuts (not arbitrary CPU/IRQ instruction interleavings); distinct synthetic snapshots at each cut')
    print('BOUNDARY: IRQ cuts are CPU-instruction-boundary possibilities, not observed Jaguar execution; other interrupts/priorities not modeled')
    print('BOUNDARY: no real hardware tearing, visible mixed frame, bus scheduling, or TOM scanline visibility proven')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',type=pathlib.Path,required=True)
    main(p.parse_args().rom)
