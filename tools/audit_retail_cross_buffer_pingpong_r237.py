#!/usr/bin/env python3
"""R237: verified original GPU stage-tail execution / CROSS buffer ownership.

Execute the original CROSS GPU stage-tail instruction bytes for all 16 runtime
stages, including Jaguar delay slots, with explicit, synthetic CPU VBlank ACKs.
The first-row register state comes from the original-instruction GPU pre-kernel
interpreter (R218), with synthetic memory pointers. No original copyrighted
payload is embedded or emitted. NOT a cycle-accurate 68000/TOM/Blitter run.
"""
from __future__ import annotations
import argparse, hashlib, pathlib, zlib
from audit_retail_cross_gpu_dispatch import extract,disassemble,verify
from audit_retail_cross_blitter_pipeline import emulate
from audit_retail_mjp_gpu_handshake import CPU as COMMAND, ROM_SHA
from audit_retail_cross_gpu_publication import EXPECTED_CPU, GPU as GPU_SITES

MASK=0xffffffff
BASE=0x8000
# Synthetic, distinct word-aligned buffer pointers from the existing R218
# interpreter; labels below are symbolic source/destination roles, NOT game RAM.
BUFFER_A=0x00050000   # first GPU command buffer / $7F84
BUFFER_B=0x000B0000   # second GPU command buffer / $7F88
SOURCE_C=0x00070000   # source old picture / $7F8C
SOURCE_D=0x00090000   # source next picture / $7F90


def audit_cpu(rom:bytes):
    dec=zlib.decompressobj(31)
    prog=dec.decompress(rom[0x3804:]);assert dec.eof and len(prog)==99072
    for addr,code in {**COMMAND,**EXPECTED_CPU}.items():
        b=bytes.fromhex(code)
        assert prog[addr-BASE:addr-BASE+len(b)]==b,hex(addr)
    # An unambiguous CPU-to-GPU command order and subsequent indirect scene
    # publication, rather than assuming that the progress publication is done.
    assert prog[0x1DFE4-BASE:0x1DFEA-BASE]==bytes.fromhex('207c00007f80')
    assert prog[0x1DFF0-BASE:0x1DFF6-BASE]==bytes.fromhex('20f90004be10')
    assert prog[0x1DFF6-BASE:0x1DFFC-BASE]==bytes.fromhex('20f90004be14')
    assert prog[0x1E0FA-BASE:0x1E102-BASE]==bytes.fromhex('23f87fa40004beec')
    print(f'PASS {len({**COMMAND,**EXPECTED_CPU})} CPU command, OP pointer and completion instruction groups')


def tail(ins:dict, r:list[int], alt:list[int], mem:dict[int,int], stage:int):
    # Start exactly at the original GPU all-144-rows-complete path. The prior
    # rows and arithmetic are independently ROM-instruction verified in R220.
    pc=0xF03472;eq=False;steps=0;writes=[];reads=[];wait_reads=0
    while pc not in (0xF030DA,0xF034CC):
        steps+=1
        if steps>100: raise AssertionError('Stage tail did not return')
        op,s,d,span,imm=ins[pc]
        nextpc=pc+span
        def execute(at:int):
            nonlocal eq,wait_reads
            x,a,b,sz,val=ins[at]
            if x==0x25:r[b]=alt[a]
            elif x==0x26:r[b]=val
            elif x==0x23:r[b]=a
            elif x==0x22:r[b]=r[a]
            elif x==0x04:r[b]=(r[b]-r[a])&MASK
            elif x==0x00:r[b]=(r[b]+r[a])&MASK
            elif x==0x02:r[b]=(r[b]+(a or 32))&MASK
            elif x==0x1F:eq=(r[b]==a)
            elif x==0x1E:eq=(r[b]==r[a])
            elif x==0x29:
                adr=r[a]
                if at==0xF034A2:
                    wait_reads+=1
                    if wait_reads==2:
                        # External synthetic CPU VBlank list copy publishes an
                        # acknowledgment between original GPU poll iterations.
                        assert mem[0x7FA8]==0
                        mem[0x7FA8]=1
                assert adr in mem,hex(adr)
                r[b]=mem[adr];reads.append((at,adr,r[b]))
            elif x==0x2F:
                adr=r[a];mem[adr]=r[b];writes.append((at,adr,r[b]))
            elif x==0x39:pass
            else:raise AssertionError(f'Unexpected original GPU opcode ${at:06X}: {x}')
            return sz
        if op in (0x34,0x35):
            if op==0x34: target=r[s]
            else: target=nextpc+(s if s<16 else s-32)*2
            taken=d==0 or (d==1 and not eq) or (d==2 and eq)
            execute(nextpc) # original Jaguar branch delay slot executes once
            nextpc=target if taken else nextpc+ins[nextpc][3]
        else:execute(pc)
        pc=nextpc
    assert wait_reads==2,(stage,wait_reads)
    assert any(a==0xF034A0 and dest==0x7FA8 and value==0 for a,dest,value in writes)
    assert pc==(0xF034CC if stage==16 else 0xF030DA),(stage,hex(pc))
    return writes,reads


def main(rompath:pathlib.Path):
    rom=rompath.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA
    audit_cpu(rom)
    img=extract(rom);ins,_=disassemble(img);verify(img,ins)
    for addr,(op,s,d,imm) in GPU_SITES.items():
        observed=ins[addr]
        assert (observed[0],observed[1],observed[2],observed[4])==(op,s,d,imm),hex(addr)
    print(f'PASS {len(GPU_SITES)} original GPU opcode checkpoints plus table hash and branch validation')
    for mode in (1,2,4,7):
        # Original GPU startup and first row-source transfer interpretation,
        # from R218. This verifies current GPU bank registers and mode-specific
        # source choice instead of hard-coding their initial states.
        _,mem,r,alt=emulate(img,ins,mode,0x4220 if mode==1 else 0x3C20)
        assert (alt[1],alt[2],alt[3],r[3],r[4],r[5]) == (BUFFER_A,BUFFER_B,0,BUFFER_B,SOURCE_C,SOURCE_D)
        expected_start=SOURCE_C if mode==1 else BUFFER_A
        assert mem[0x7FA4]==expected_start
        publication=[];all_writes=[];previous_destination=BUFFER_B
        for stage in range(1,17):
            r[20]=stage
            # Original $F030E4 publishes the stage counter *before* 144 rows.
            r[12]=0x7FB4;r[13]=r[20];assert ins[0xF030E4][:3]==(0x2F,12,13)
            mem[r[12]]=r[13]
            before=(mem[0x7FA4],r[3]);assert before[1]==previous_destination
            after,rds=tail(ins,r,alt,mem,stage)
            pubs=[(at,v) for at,a,v in after if a==0x7FA4]
            expected=BUFFER_B if stage&1 else BUFFER_A
            assert pubs==([(0xF03484,BUFFER_A),(0xF03490,BUFFER_B)] if stage==1 else [(0xF03490,expected)]),(mode,stage,pubs)
            assert mem[0x7FA4]==expected,(mode,stage)
            assert r[3]==(BUFFER_A if stage&1 else BUFFER_B)
            assert mem[0x7FA8]==1 and mem[0x7FB4]==stage
            # At original stage boundaries GPU waits for a VBlank-mediated
            # CPU acknowledgment; it does not increment stage at writeback.
            assert [at for at,a,_ in after if a==0x7FA8]==[0xF034A0]
            publication.append((stage,before[0],before[1],expected,r[3],len(pubs)))
            all_writes.extend(pubs)
            previous_destination=r[3]
        assert sum(row[5] for row in publication)==17
        assert [row[3] for row in publication]==[BUFFER_B if p&1 else BUFFER_A for p in range(1,17)]
        assert len(publication)==16
        print('PASS mode',mode,'initial OP buffer',hex(expected_start),'stage-1 double write, then 16 alternating published buffers')
        if mode==2:
            for phase,prev,dst,published,nextdst,count in publication:
                print(f'  stage {phase:02d}: publish ${published:06X}, next GPU destination ${nextdst:06X}, writes {count}')
    # Status 2 only follows the original final-stage VBlank acknowledgment.
    assert ins[0xF034F4][:3]==(0x23,2,13)
    assert ins[0xF034F6][:3]==(0x2F,12,13)
    print('PASS original stage-completion status 2 is downstream of stage 16 ACK; first progress=1 precedes any completed stage')
    print('LIMIT: external CPU VBlank ACK is deliberately synthetic, not a coupled hardware interrupt/TOM scanline trace')
    print('LIMIT: ORIGINAL image content and source buffer addresses at runtime depend on frontend setup; synthetic pointers are labels')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=pathlib.Path,required=True)
    main(p.parse_args().rom)
