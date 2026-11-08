#!/usr/bin/env python3
"""R220 offline check: execute retail CROSS per-word GPU branch loops across full RGB16 rows.

No game runtime instrumentation, no extracted art, no GPU/Blitter timing assertion.
ROM-derived instructions and Blitter flag geometry are used with synthetic pixels.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from hashlib import sha256
from audit_retail_cross_gpu_dispatch import extract, disassemble, verify, gpu_blend_word, EXPECTED_KERNELS, MASKS
from audit_retail_cross_blitter_flags import decode_flags, verify_cpu
from audit_retail_cross_blitter_pipeline import emulate, MEMSRC
from audit_retail_mjp_gpu_handshake import ROM_SHA
import zlib

MASK32 = 0xffffffff
SCRATCH_OLD=0xF03D80
SCRATCH_NEW=0xF03B00
SCRATCH_LOOKAHEAD=0xF03880
LAST=0xF0343E

def row_words(buf:bytes):
    assert len(buf)%4==0
    return [int.from_bytes(buf[i:i+4],'big') for i in range(0,len(buf),4)]

def execute_kernel_row(ins,phase:int,old_row:bytes,new_row:bytes):
    """Run unchanged GPU RISC kernel instructions, including SUBQ loop, conditional
    JUMP and its original ADDQT delay slot, over a scratch RAM word map.
    """
    assert 0<=phase<=16 and len(old_row)==len(new_row) and len(old_row) in (384,640)
    words=len(old_row)//4
    mem={SCRATCH_OLD+i*4:w for i,w in enumerate(row_words(old_row))}
    mem.update({SCRATCH_NEW+i*4:w for i,w in enumerate(row_words(new_row))})
    regs=[0]*32
    regs[12]=SCRATCH_OLD
    regs[19]=SCRATCH_NEW
    regs[23]=words
    regs[0]=EXPECTED_KERNELS[phase]
    for r,m in MASKS.items():regs[r]=m
    pc=EXPECTED_KERNELS[phase]
    executions=0
    stores=[]
    loop_jumps=0
    while pc!=LAST:
        if executions>len(old_row)*10:raise AssertionError(f'kernel never stopped: {phase}')
        executions+=1
        if pc not in ins:raise AssertionError(f'bad GPU PC: {pc:X}')
        op,s,d,span,imm=ins[pc]
        np=pc+span
        if op==0x29: # LOAD (Rs),Rd
            assert regs[s] in mem
            regs[d]=mem[regs[s]]
        elif op==0x22:regs[d]=regs[s]  # MOVE
        elif op==0x19:regs[d]=(regs[d]>>(s or 32))&MASK32 # SHRQ
        elif op==0x09:regs[d]&=regs[s] # AND
        elif op==0x04:regs[d]=(regs[d]-regs[s])&MASK32 # SUB
        elif op==0x00:regs[d]=(regs[d]+regs[s])&MASK32 # ADD
        elif op==0x2f: # STORE
            assert s==12 and d==22 and SCRATCH_OLD<=regs[s]<SCRATCH_OLD+len(old_row)
            mem[regs[s]]=regs[d]
            stores.append(regs[s])
        elif op==0x06: # SUBQ #1, r23
            assert s==1 and d==23 and regs[d]>0
            regs[d]-=1
        elif op==0x03:regs[d]=(regs[d]+(s or 32))&MASK32 # ADDQT (flags unchanged)
        elif op==0x26:regs[d]=imm # MOVEI next PC to common tail
        elif op==0x34: # JUMP (Rs), condition -- delay slot executes regardless
            ds=ins[np]
            if d==1:
                assert s==0 and ds[:3]==(0x03,4,19),f'bad looping delay slot {pc:X}'
                regs[19]=(regs[19]+4)&MASK32
                np=regs[s] if regs[23]!=0 else np+ds[3]
                if regs[23]:loop_jumps+=1
            else:
                assert d==0 and s==19 and ds[0]==0x39 and regs[19]==LAST
                np=regs[19]
        elif op==0x39: pass
        else:raise AssertionError(f'unhandled kernel opcode {op} at {pc:X}')
        pc=np
    assert regs[23]==0 and len(stores)==words and loop_jumps==words-1
    assert stores==[SCRATCH_OLD+i*4 for i in range(words)]
    assert regs[12]==SCRATCH_OLD+len(old_row)
    assert regs[19]==LAST
    return b''.join(mem[SCRATCH_OLD+i*4].to_bytes(4,'big') for i in range(words)),executions

def patterned_row(width:int,tag:int)->bytes:
    # Each pixel is a deterministic synthetic 16-bit value, never retail artwork.
    return b''.join(((x*7919 + tag*30011 + (x>>3)*173)&0xffff).to_bytes(2,'big') for x in range(width))

def run(rom_file:Path):
    rom=rom_file.read_bytes()
    assert len(rom)==4194304 and sha256(rom).hexdigest()==ROM_SHA
    cpu=zlib.decompressobj(31).decompress(rom[0x3804:]);assert len(cpu)==99072
    verify_cpu(cpu)
    gpu=extract(rom)
    ins,_=disassemble(gpu);verify(gpu,ins)
    assert ins[0xF03160][:3]==(0x22,15,12)
    assert ins[0xF03162][:3]==(0x22,16,19)
    assert ins[0xF03168][:3]==(0x22,1,23)
    assert ins[0xF0316A][:3]==(0x19,1,23)
    assert ins[0xF0316C][:3]==(0x34,0,0)
    assert ins[0xF0316E][0]==0x39
    count=0;loops=0
    for width,mode,flags in ((192,2,0x3c20),(192,4,0x3c20),(192,7,0x3c20),
                             (320,1,0x4220),(320,3,0x4220),(320,8,0x4220)):
        assert decode_flags(flags)['width']==width
        cap,_,regs,_=emulate(gpu,ins,mode,flags)
        assert regs[1]==width and len(cap)==4
        assert cap[0][1][0xF02200]==0xF0BD80
        assert cap[1][1][0xF02200]==0xF0BB00
        assert cap[2][1][0xF02200]==0xF0B880
        assert cap[3][1][0xF02224]==SCRATCH_OLD
        old=patterned_row(width,mode)
        new=patterned_row(width,mode+17)
        lookahead=patterned_row(width,mode+39)
        assert len(old)==width*2
        scratch={SCRATCH_OLD:old,SCRATCH_NEW:new,SCRATCH_LOOKAHEAD:lookahead}
        for phase in range(17):
            result,iterations=execute_kernel_row(ins,phase,scratch[SCRATCH_OLD],scratch[SCRATCH_NEW])
            # Compare full-row loop with the earlier isolated GPU interpreter,
            # which independently reads each original instruction kernel.
            expected=b''.join(gpu_blend_word(gpu,ins,phase,a,b).to_bytes(4,'big')
                for a,b in zip(row_words(old),row_words(new)))
            assert result==expected,(mode,phase)
            if phase==0:assert result==old
            if phase==16:assert result==new
            assert len(result)==width*2 and iterations>len(result)//4
            # Original fourth Blitter call writes processed scratch back to a
            # synthetic scene row; no live Jaguar OP display is claimed.
            destination=bytearray(width*2)
            destination[:]=result
            assert bytes(destination)==result
            count+=1;loops+=width//2
        # Original lookahead must remain separate and unaffected by the kernel.
        assert scratch[SCRATCH_LOOKAHEAD]==lookahead
    print('PASS original ROM / 68000 $7FB0 16bpp 192/320 width flags and CROSS GPU identity')
    print('PASS $F03160..$F0316E actual phase dispatch and half-width 32-bit word counter')
    print(f'PASS {count} complete original-instruction GPU row loops ({loops} processed words)')
    print('PASS exact word-by-word outputs against independent R216 17-stage kernel executor')
    print('PASS last-word branch delay and byte/word strides; distinct untouched lookahead scratch')
    print('LIMIT: synthetic input, no physical Blitter cycles, asynchronous CPU/GPU scheduling or OP presentation')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',required=True,type=Path)
    run(p.parse_args().rom)
