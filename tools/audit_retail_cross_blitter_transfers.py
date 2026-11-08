#!/usr/bin/env python3
"""Read-only Atari Jaguar AVP retail CROSS GPU Blitter command verification.

Executes only the verified $F0357C..$F035CA Blitter setup subroutine
against synthetic registers; no production hooks, ROM assets or scene output.
The standalone audit is NOT a full GPU emulation or an OP timing trace.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from audit_retail_cross_gpu_dispatch import extract, disassemble, rd16

START = 0xF0357C
END = 0xF035CC
GPU_ALIAS = 0x8000
SCRATCH = (0xF03880, 0xF03B00, 0xF03D80)
PHRASE_BYTES = 0x280
BLIT_REGISTERS = (
    (0xF02200, 'A1_BASE'),
    (0xF02204, 'A1_FLAGS'),
    (0xF0220C, 'A1_PIXEL'),
    (0xF02210, 'A1_STEP'),
    (0xF02224, 'A2_BASE'),
    (0xF02228, 'A2_FLAGS'),
    (0xF02230, 'A2_PIXEL'),
    (0xF02234, 'A2_STEP'),
    (0xF0223C, 'B_COUNT'),
    (0xF02238, 'B_CMD'),
)
# Packed Jaguar GPU bit fields (op6/reg1-5/reg2-5).
OP_MOVEI=0x26
OP_STORE=0x2F
OP_ADDQ=0x02
OP_SUBQ=0x06
OP_LOAD=0x29
OP_MOVE=0x22
OP_MOVEQ=0x23
OP_SHLQ=0x18
OP_OR=0x0A
OP_JUMP=0x34


def run_one(image: bytes, insns: dict, width: int, flags: int, y: int) -> list[tuple[int,int]]:
    """Execute unchanged source instruction bytes, synthetic input registers."""
    reg=[0]*32
    reg[1]=width
    reg[6]=(y<<16)
    reg[7]=0
    reg[10]=0x00040000
    reg[11]=0x00F0BD80
    reg[18]=0x00F03114
    ram={0x7FB0:flags}
    writes=[]
    pc=START
    for _ in range(80):
        if pc not in insns:
            raise AssertionError(f'Not a valid original GPU instruction ${pc:06X}')
        op,s,d,span,imm=insns[pc]
        if op==OP_MOVEI:
            reg[d]=imm
        elif op==OP_STORE:
            writes.append((reg[s],reg[d]))
        elif op==OP_ADDQ:
            reg[d]=(reg[d]+(s or 32))&0xFFFFFFFF
        elif op==OP_SUBQ:
            reg[d]=(reg[d]-(s or 32))&0xFFFFFFFF
        elif op==OP_LOAD:
            if reg[s] not in ram:raise AssertionError(f'Unexpected GPU RAM read ${reg[s]:X}')
            reg[d]=ram[reg[s]]
        elif op==OP_MOVE:
            reg[d]=reg[s]
        elif op==OP_MOVEQ:
            reg[d]=s
        elif op==OP_SHLQ:
            reg[d]=(reg[d]<<(s or 32))&0xFFFFFFFF
        elif op==OP_OR:
            reg[d]=reg[d]|reg[s]
        elif op==OP_JUMP:
            if reg[s]!=0xF03114 or d!=0:
                raise AssertionError('Unexpected GPU return register')
            if pc!=0xF035C8:
                raise AssertionError('Blitter command did not reach return')
            # Branch delay slot at $F035CA is a NOP; verify it explicitly.
            if rd16(image,pc+2)!=0xE400:
                raise AssertionError('Missing original JUMP delay slot NOP')
            break
        else:
            raise AssertionError(f'Unhandled original RISC opcode {op:#x} at ${pc:06X}')
        pc+=span
    else:
        raise AssertionError('No original GPU return reached')
    got=tuple(addr for addr,_ in writes)
    want=tuple(addr for addr,_ in BLIT_REGISTERS)
    if got!=want:
        raise AssertionError(f'Original GPU register write ordering mismatch {got} != {want}')
    expected=(0xF0BD80,flags,0,0,0x40000,flags,y<<16,0,(1<<16)|width,0x01800601)
    vals=tuple(value for _,value in writes)
    if vals!=expected:
        raise AssertionError(f'Original Blitter register data mismatch {vals} != {expected}')
    return writes


def run(rom: Path) -> None:
    image=extract(rom.read_bytes())
    insns,_=disassemble(image)
    # The original 4K on-chip RAM ends immediately after the last 640-byte
    # scratch region. These are buffers, not embedded ROM data or host image.
    assert SCRATCH[0]>=0xF03000 and SCRATCH[-1]+PHRASE_BYTES==0xF04000
    assert all(b-a==PHRASE_BYTES for a,b in zip(SCRATCH,SCRATCH[1:]))
    assert (0xF03000+len(image))<SCRATCH[0]
    # Original GPU call sites, poll/wait loops, source/destination setup; each
    # uses an indirect call to same helper in R29 and return label in R18.
    callers=((0xF03110,0xF03114,0xF030FC),
             (0xF03134,0xF03138,0xF03120),
             (0xF0315C,0xF03160,0xF03148),
             (0xF0345C,0xF03460,0xF03448))
    for call, ret, setup in callers:
        assert insns[call][:3]==(OP_JUMP,29,0)
        assert insns[setup][:3]==(OP_MOVEI,0,18)
        assert insns[setup][4]==ret
        assert rd16(image,call+2)==0xE400 # delay-slot NOP
    # GPU input mode at $7FAC chooses one exact row width.  Condition value
    # 2 means Z=1, so an equal comparison skips the fallback 320 width.
    assert insns[0xF03038][:3]==(OP_MOVEI,0,1)
    assert insns[0xF03038][4]==192
    assert insns[0xF03058][:3]==(OP_MOVEI,0,1)
    assert insns[0xF03058][4]==320
    for at, n in ((0xF03046,2),(0xF0304C,4),(0xF03052,7)):
        assert insns[at][:3]==(0x1F,n,13)  # CMPQ #n,R13
        jr=insns[at+2]
        assert jr[0]==0x35 and jr[2]==2 # JR Z
        disp=jr[1] if jr[1]<16 else jr[1]-32
        assert at+4+disp*2==0xF0305E
    # Literal buffer offsets establish the 32-bit-write GPU-RAM alias.
    assert insns[0xF0305E][4]==0xF03D80
    assert insns[0xF03064][4]==0xF03B00
    assert insns[0xF0306A][4]==0xF03880
    assert insns[0xF03088][4]==GPU_ALIAS
    assert insns[0xF03008][:3]==(OP_MOVEI,0,29)
    assert insns[0xF03008][4]==START
    for width in (192,320):
        for y in (0,1,143):
            for flags in (0,0x00001438,0xFFFFFFFF):
                writes=run_one(image,insns,width,flags,y)
                vals={addr:v for addr,v in writes}
                assert (vals[0xF0223C]>>16,vals[0xF0223C]&0xFFFF)==(1,width)
                assert ((vals[0xF02238]>>21)&15)==12 # LFU_REPLACE
    cmd=0x01800601
    assert tuple(i for i in range(32) if cmd&(1<<i))==(0,9,10,23,24)
    print('PASS original GPU subroutine $F0357C..$F035CA: 10 exact ordered Blitter-register writes')
    print('PASS 4 original wait-and-call sites; exact return labels and branch-delay NOPs')
    print('PASS $7FAC mode 2/4/7 selects 192 pixels; other modes select 320 pixels')
    print('PASS hardware register meanings: A1/A2 BASE, FLAGS, PIXEL, STEP, B_COUNT, B_CMD')
    print('PASS 18 synthetic original-instruction evaluations (2 widths x 3 Y x 3 flags)')
    print('PASS outer count=1, inner count=192/320, SRCEN and LFU_REPLACE, UPDA1/UPDA2')
    print('PASS GPU scratch $F03880,$F03B00,$F03D80 x $280 bytes fill GPU RAM to $F04000')
    print('BOUNDARY: flags at $7FB0, full Blitter DMA, GPU register scheduling and OP publication need runtime proof')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    run(p.parse_args().rom)
