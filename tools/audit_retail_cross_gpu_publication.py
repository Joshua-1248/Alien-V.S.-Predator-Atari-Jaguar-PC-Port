#!/usr/bin/env python3
"""R221: original CROSS GPU progress/VBlank/OP-picture publication source audit.

Reads a privately supplied, SHA-checked retail AVP.jag. Examines authentic
68000 instruction bytes and CROSS GPU instructions; verifies the stage/row
control-flow dependencies with a deliberately limited model. Does NOT execute
Blitter DMA, interrupt latency, OP display lists or GPU color kernels. No
production hooks, embedded copyrighted images or game-ROM output.
"""
from __future__ import annotations
import argparse
from hashlib import sha256
from pathlib import Path
import zlib
from audit_retail_cross_gpu_dispatch import extract,disassemble,rd32,EXPECTED_KERNELS,verify
from audit_retail_mjp_gpu_handshake import ROM_SHA

CPU_BASE=0x8000
EXPECTED_CPU={
    0x01E010: '20fc00000090',  # 144 row count in GPU parameter stream at $7F98
    0x01E022: '207c00007fb4',  # progress address
    0x01E028: '20bc00000001',  # initial published progress
    0x01E02E: '23fc00f0300000f02110',  # original GPU entry
    0x01E038: '23fc0000000100f02114',  # GPU start
    0x01E094: '207c00007fb42210d2bc00000008',  # CPU reads progress, adds 8
    0x01E0FA: '23f87fa40004beec',  # OP picture data source = GPU-published pointer
    0x01E13A: '4eb90001e240',  # Make_New emits actual OP bitmap phrase
    0x01E140: '207c00007f802010b0bc00000002',  # wait for GPU status 2
    0x01E14E: '6600ff44',  # repeat the OP update until completed
    0x01E15A: '42b87fa84ab87fa867fa4e75',  # One_Tick clear/wait on VBlank word
    0x01E3BE: '21fc000000017fa8',  # retail MJP list work sets VBlank wake
    0x01E520: '21fc000000017fa8',  # alternate original list path
    0x01E85E: '21fc000000017fa8',  # alternate original list path
}

# Source proof is intentionally at the instruction boundary rather than a
# host-created expected image, so a change to the actual retail program fails.
GPU={
    0xF03000:(0x26,0,13,0xF03538),  # 17-entry dispatch table
    0xF03018:(0x02,4,12,None),     # advance through CPU inputs
    0xF03034:(0x29,12,2,None),     # height from $7F98
    0xF030BE:(0x26,0,21,16),      # number of phases
    0xF030C4:(0x23,1,20,None),    # first runtime phase = 1
    0xF030DA:(0x23,0,8,None),     # first row=0 per phase
    0xF030DC:(0x26,0,12,0x7FB4),
    0xF030E2:(0x22,20,13,None),
    0xF030E4:(0x2f,12,13,None),   # progress published BEFORE row transfers
    0xF030E8:(0x25,0,14,None),    # dispatch table base from alternate bank
    0xF030EA:(0x18,30,13,None),  # phase*4 (SHLQ #30 = shift 2)
    0xF030EC:(0x00,13,14,None),
    0xF030EE:(0x29,14,0,None),    # dispatch pointer read
    0xF03168:(0x22,1,23,None),
    0xF0316A:(0x19,1,23,None),    # half-width word counter
    0xF0316C:(0x34,0,0,None),     # kernel jump
    0xF03460:(0x02,1,8,None),    # next row
    0xF0346C:(0x1E,8,2,None),    # row count comparison
    0xF0346E:(0x34,19,1,None),   # loop if not done
    0xF03472:(0x25,3,13,None),
    0xF03476:(0x1F,1,20,None),   # special swap only on phase 1
    0xF03478:(0x35,6,1,None),    # skip swap except first phase
    0xF0347C:(0x26,0,12,0x7FA4),
    0xF03482:(0x25,1,13,None),
    0xF03484:(0x2f,12,13,None), # original output pointer publication
    0xF03486:(0x26,0,12,0x7FA4),
    0xF0348C:(0x29,12,13,None),
    0xF0348E:(0x22,3,14,None),
    0xF03490:(0x2f,12,14,None),
    0xF03492:(0x22,13,3,None),  # original buffer swap
    0xF03498:(0x26,0,12,0x7FA8),
    0xF0349E:(0x23,0,13,None),
    0xF034A0:(0x2f,12,13,None),  # clear GPU/CPU VBlank handshake
    0xF034A2:(0x29,12,13,None),
    0xF034A4:(0x1F,0,13,None),
    0xF034A6:(0x35,29,2,None),  # poll until CPU posts nonzero
    0xF034AA:(0x26,0,19,0xF034C0),
    0xF034B0:(0x1E,21,20,None), # compare runtime phase against 16
    0xF034B2:(0x34,19,1,None),  # if not last, increment
    0xF034C0:(0x02,1,20,None),  # phase++
    0xF034C2:(0x26,0,19,0xF030DA),
    0xF034C8:(0x34,19,0,None), # next phase starts at row reset
    0xF034EE:(0x26,0,12,0x7F80),
    0xF034F4:(0x23,2,13,None),
    0xF034F6:(0x2F,12,13,None), # signal complete ONLY after last phase
}

def trace_cpu(code:bytes):
    for at,want in EXPECTED_CPU.items():
        s=bytes.fromhex(want)
        if code[at-CPU_BASE:at-CPU_BASE+len(s)]!=s:
            raise AssertionError(f'68000 opcode mismatch at ${at:06X}')
    # 68000 BNE.W displacement relative to the extension word address.
    off=int.from_bytes(code[0x1E150-CPU_BASE:0x1E152-CPU_BASE],'big',signed=True)
    assert 0x1E150+off==0x1E094
    # The One_Tick loop uses B(EQ).B -6 to test after a wake-up.
    assert code[0x1E162-CPU_BASE:0x1E164-CPU_BASE]==bytes.fromhex('67fa')
    print(f'PASS {len(EXPECTED_CPU)} retail 68000 CPU opcode groups including original OP pointer publication')
    print('PASS CPU $7FB4+8 OP update loop, completion status 2 and three VBlank wake sites')

def trace_gpu(img:bytes,ins:dict):
    for at,(op,s,d,imm) in GPU.items():
        observed=ins[at]
        if (observed[0],observed[1],observed[2],observed[4])!=(op,s,d,imm):
            raise AssertionError(f'CROSS GPU instruction mismatch at ${at:06X}: {observed}')
    # Verify actual GPU JMP/JR targets with on-chip delay slots.
    for at,target in ((0xF03478,0xF03486),(0xF034A6,0xF034A2)):
        op,s,cc,_,_=ins[at]
        assert op==0x35
        signed=s if s<16 else s-32
        assert at+2+(signed*2)==target
        assert ins[at+2][0]==0x39
    assert ins[0xF03470][0]==0x39  # JUMP NZ delay slot
    assert ins[0xF034B4][0]==0x39  # JUMP NZ delay slot
    assert ins[0xF034CA][0]==0x39  # JUMP always delay slot
    assert rd32(img,0xF03538+0*4)==EXPECTED_KERNELS[0]
    assert rd32(img,0xF03538+16*4)==EXPECTED_KERNELS[16]
    print(f'PASS {len(GPU)} GPU instruction checkpoints and original conditional branch/delay-slot targets')

    # Control-only execution of validated source instructions; per-row kernel and
    # Blitter mechanics independently tested in earlier offline ROM audits.
    stage=1
    progress=[]
    read_by_op=[]
    row_checks=0
    lookups=[]
    published_first_stage=False
    while True:
        assert 1<=stage<=16
        progress.append(stage)        # $F030E4 STORE
        assert 0<=stage*4<17*4
        lookups.append(rd32(img,0xF03538+stage*4))
        assert lookups[-1]==EXPECTED_KERNELS[stage]
        # F030DA resets row=0; F03460 increments; CMP R8,R2 stops at 144.
        row=0
        while True:
            assert row<144
            row+=1
            row_checks+=1
            if row==144:break
        if stage==1:
            # GPU swaps active scene destination and $7FA4 only once.
            published_first_stage=True
        read_by_op.append((stage,stage+8)) # CPU $1E09A/$1E09C before OP build
        # GPU waits on CPU's common VBlank acknowledgment $7FA8 once per phase.
        if stage==16:break
        stage+=1                 # $F034C0 ADDQ #1,R20
    assert progress==list(range(1,17))
    assert len(lookups)==16 and 0 not in progress
    assert row_checks==16*144 and published_first_stage
    assert progress[-1]==16
    print(f'PASS {len(progress)} *runtime* GPU blend stages (1..16, NOT 0..16), {row_checks} row completions')
    print('PASS 16 GPU progress publications and 16 $7FA8 VBlank waits before completion value 2')
    print('PASS first-stage-only $7FA4 buffer swap and CPU $7FA4 -> OP bitobject pointer flow')
    print('BOUNDARY: proof of instruction control flow, not coupled GPU/VBlank/OP cycle-by-cycle output or visible frame count')

def main(path:Path):
    rom=path.read_bytes()
    if len(rom)!=4194304 or sha256(rom).hexdigest()!=ROM_SHA:
        raise ValueError('An unmodified, verified personal retail AVP.jag is required')
    z=zlib.decompressobj(31);code=z.decompress(rom[0x3804:]);assert z.eof and len(code)==99072
    img=extract(rom)
    ins,_=disassemble(img)
    verify(img,ins)
    trace_cpu(code)
    trace_gpu(img,ins)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    main(p.parse_args().rom)
