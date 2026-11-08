#!/usr/bin/env python3
"""R218: original CROSS GPU pre-kernel row-DMA register-flow verifier.

Interprets *original* GPU instruction bytes, both GPU register banks, exact
conditional JR and JUMP delay slots, and the original Blitter helper. Synthetic
host test RAM is NOT original runtime data. No production instrumentation.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import zlib
from hashlib import sha256
from audit_retail_mjp_gpu_handshake import CPU, ROM_SHA
from audit_retail_cross_gpu_dispatch import extract,disassemble,rd32


MASK32=0xffffffff
MEMSRC=(0x70000,0x90000,0xb0000)
CALLS=(0xF03110,0xF03134,0xF0315C)
BLITREG=(0xF02200,0xF02204,0xF0220C,0xF02210,0xF02224,0xF02228,0xF02230,0xF02234,0xF0223C,0xF02238)

def run_helper(ins, r, mem, return_to):
    """Original $F0357C helper with caller registers, including direction fields."""
    regs=r[:]  # Execute helper on caller register state; original helper clobbers are retained.
    writes=[];pc=0xF0357C
    for _ in range(72):
        op,a,d,span,imm=ins[pc]
        if op==0x26:regs[d]=imm
        elif op==0x2f:writes.append((regs[a],regs[d]))
        elif op==0x02:regs[d]=(regs[d]+(a or 32))&MASK32
        elif op==0x06:regs[d]=(regs[d]-(a or 32))&MASK32
        elif op==0x29:regs[d]=mem[regs[a]]
        elif op==0x22:regs[d]=regs[a]
        elif op==0x23:regs[d]=a
        elif op==0x18:regs[d]=(regs[d]<<(32-a))&MASK32
        elif op==0x0a:regs[d]|=regs[a]
        elif op==0x34:
            assert pc==0xF035C8 and regs[a]==return_to and d==0
            assert ins[pc+2][0]==0x39
            assert tuple(addr for addr,_ in writes)==BLITREG
            return writes,regs
        else:raise AssertionError(f'Unhandled helper opcode {op} at ${pc:X}')
        pc+=span
    raise AssertionError('GPU helper did not return')

def emulate(romgpu:bytes,ins:dict,mode:int,flags:int):
    # Arbitrary synthetic pointers (explicitly not assumed cartridge RAM values).
    r=[0]*32; alt=[0]*32; pc=0xF03000; zero=False
    mem={0x7f84:0x00050000,0x7f88:MEMSRC[2],0x7f8c:MEMSRC[0],
         0x7f90:MEMSRC[1],0x7f94:0,0x7f98:144,
         0x7fac:mode,0x7fb0:flags,0xF02238:1,
         0xF00050:0xffffffff}
    captures=[]
    def step(addr:int):
        nonlocal zero
        op,s,d,span,imm=ins[addr]
        dest=addr+span
        if op==0x39:pass
        elif op==0x26: r[d]=imm
        elif op==0x23:r[d]=s
        elif op==0x22:r[d]=r[s]
        elif op==0x24:alt[d]=r[s]
        elif op==0x25:r[d]=alt[s]
        elif op==0x29:
            if r[s] not in mem and not (0xF03538<=r[s]<=0xF03578 and r[s]%4==0):
                raise AssertionError(f'Unprovided GPU memory ${r[s]:X} at ${addr:X}')
            r[d]=rd32(romgpu,r[s]) if 0xF03538<=r[s]<=0xF03578 else mem[r[s]]
        elif op==0x2f:mem[r[s]]=r[d]
        elif op==0x02:r[d]=(r[d]+(s or 32))&MASK32
        elif op==0x06:r[d]=(r[d]-(s or 32))&MASK32
        elif op==0x00:r[d]=(r[d]+r[s])&MASK32
        elif op==0x18:r[d]=(r[d]<<(32-s))&MASK32
        elif op==0x19:r[d]=(r[d]>>(s or 32))&MASK32
        elif op==0x1f:zero=r[d]==(s or 32)
        elif op==0x0d:zero=(r[d] & (1<<s))==0
        elif op in (0x34,0x35):
            # Real branch delay slot must execute with the original PC values.
            if op==0x34: target=r[s]
            else:
                signed=s if s<16 else s-32
                target=dest+2*signed
            taken=(d==0 or (d==1 and not zero) or (d==2 and zero))
            delay=dest
            if delay not in ins:raise AssertionError(f'No GPU delay slot after ${addr:X}')
            dop,ds,dd,_,_=ins[delay]
            if dop in (0x34,0x35):raise AssertionError('Nested branch delay')
            step(delay)
            dest=target if taken else delay+ins[delay][3]
        else:raise AssertionError(f'Unhandled GPU instruction opcode {op} at ${addr:X}')
        return dest
    for _ in range(240):
        if pc==0xF0316C:
            if len(captures)!=3:raise AssertionError('Blend dispatch before all three transfers')
            assert r[12]==0xF03D80 and r[19]==0xF03B00
            assert r[15]==0xF03880 and r[17]==0xF03D80
            # Skip the GPU blend kernel and word-loop: their arithmetic is
            # independently verified in R216; full DMA is NOT interpreted.
            # The proven common post-kernel block starts at $F0343E.
            # Parent r3 holds original $7F88 destination scene buffer pointer.
            pc=0xF0343E
        if pc==0xF0345C:
            if r[29]!=0xF0357C or r[18]!=0xF03460:
                raise AssertionError('Final original Blitter helper call/return')
            writes,regs=run_helper(ins,r,mem,0xF03460)
            captures.append((pc,{a:v for a,v in writes},r[8]))
            return captures,dict(mem),r,alt
        if pc in CALLS:
            if r[29]!=0xF0357C or r[18]!=pc+4:
                raise AssertionError('Original helper/return label mismatch')
            writes,regs=run_helper(ins,r,mem,pc+4)
            captures.append((pc,{a:v for a,v in writes},r[8]))
            # Preserve only legitimate helper register effects (the original
            # return also executes a NOP in its branch-delay slot).
            r[:]=regs
            pc+=4
            continue
        pc=step(pc)
    raise AssertionError(f'Did not reach kernel dispatch: ${pc:X}, transfers {len(captures)}')

def run(path:Path):
    rom=path.read_bytes()
    assert len(rom)==4194304 and sha256(rom).hexdigest()==ROM_SHA
    cpu=zlib.decompressobj(31).decompress(rom[0x3804:])
    assert len(cpu)==99072
    # This is the *CPU's* actual low-RAM command layout; no guessed GPU inputs.
    cpu_sites=(0x1dfe4,0x1dfea,0x1dff0,0x1dff6,0x1dffc,0x1dffe,
               0x1e004,0x1e008,0x1e00a,0x1e010,0x1e022,0x1e028)
    for site in cpu_sites:
        want=bytes.fromhex(CPU[site]); got=cpu[site-0x8000:site-0x8000+len(want)]
        assert got==want,f'Original 68000 command word ${site:X} mismatch'
    gpu=extract(rom);ins,_=disassemble(gpu)
    for mode in (1,2,3,4,7,8):
        for flags in (0x00010820,0x00014220):
            transfers,mem,r,alt=emulate(gpu,ins,mode,flags)
            width=192 if mode in (2,4,7) else 320
            assert r[1]==width
            assert [x[0] for x in transfers]==list(CALLS)+[0xF0345C]
            expected=((MEMSRC[0],0xF0BD80,0),
                      (MEMSRC[1],0xF0BB00,0),
                      (MEMSRC[0],0xF0B880,1),
                      (0xF03D80,MEMSRC[2],0))
            for i,(_,regs,cur_y) in enumerate(transfers):
                src,dst,row=expected[i]
                assert regs[0xF02200]==dst and regs[0xF02224]==src,(mode,i,regs)
                assert regs[0xF02230]==(row<<16 if i<3 else 0)
                assert regs[0xF0220C]==0
                assert regs[0xF02204]==regs[0xF02228]==flags
                assert regs[0xF0223C]==(1<<16)|width
            # Destination and scratch pointers: not inferred from visual art.
            assert mem[0x7fa0]==0 and mem[0x7fb4]==1
            assert mem[0x7fa4]==(0x50000 if mode!=1 else MEMSRC[0])
    print('PASS 12 original 68000 low-RAM command-parameter instruction groups')
    print('PASS original GPU dual-register-bank / conditional-branch and delay-slot execution')
    print('PASS 12 paired scenario evaluations covering scene modes 1,2,3,4,7,8')
    print('PASS three pre-kernel source->scratch transfers and final scratch->scene-buffer writeback')
    print('PASS one-row lookahead, shared flag propagation, width branch, original progress and phase')
    print('BOUNDARY: $7FB0 is supplied by synthetic tests, NOT recovered retail RAM initialization')
    print('BOUNDARY: GPU per-word arithmetic/loop and physical Blitter/OP DMA not fully emulated')

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',required=True,type=Path)
    run(ap.parse_args().rom)
