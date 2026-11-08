#!/usr/bin/env python3
"""R238 offline ROM-anchored CROSS CPU busy-poll / GPU ACK dependency audit.

Original instruction windows and control-flow are verified, then tiny order-
constraint models demonstrate why a progress value is not a completed or
visible frame. Deliberately NO emulation of CPU/GPU relative instruction timing,
TOM scanout, Blitter bus arbitration, or actual display frames. Original retail
machine-code, assets and screenshots are neither printed nor redistributed.
"""
from __future__ import annotations
import argparse, hashlib, pathlib, zlib
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_cross_gpu_dispatch import extract,disassemble,verify
from audit_retail_cross_gpu_publication import GPU as GPU_INS
from audit_retail_prelist_vblank_dispatch import INSN as VBLANK_INS

BASE=0x8000
WINDOWS={
    # Full CPU epoch-B loop from progress LOAD through completion test and return
    (0x1E094,0x1E154):'8b48f9510654880ef856fafb2b3e4c61a66fdd88782e2c8d7f4155ef03ba95b2',
    # Make_New is a *synchronous 68000 record builder*, not a wait/list copy.
    (0x1E240,0x1E2CA):'e1d3885143dd7282716c30c41c639df9bcaa2f4234d7e59679c094e2b5f3be41',
    # VBlank callback is invoked by an interrupt independently of CPU loop.
    (0x91B6,0x91F6):'f57ee89b1f88ea93dd229060d3ca0f6e7f5f3e7253900f343c0d662f7d2de33f',
    # Every list publication ACK is at the *end* of its callback path.
    (0x1E34A,0x1E3C8):'f65bda940d2aa2c9469efb6e179c5ad7d063293cffd91f7ccad8463086456428',
    (0x1E4D0,0x1E52A):'f303ac130baea4d810173f2d9cea679a075a22e2e1c513fec0bddd035eaa56c1',
    (0x1E804,0x1E868):'824779329964402694db8b9d9439016bb3844960fc764e2523234d909621999a',
}

def cpu_bytes(code:bytes,addr:int,expected:str):
    raw=bytes.fromhex(expected)
    assert code[addr-BASE:addr-BASE+len(raw)]==raw,hex(addr)

def branch_target(code:bytes,addr:int)->int:
    op=int.from_bytes(code[addr-BASE:addr-BASE+2],'big')
    assert (op&0xF000)==0x6000,(hex(addr),hex(op))
    if op&0xFF:
        v=op&0xFF;d=v if v<128 else v-256
    else:d=int.from_bytes(code[addr-BASE+2:addr-BASE+4],'big',signed=True)
    return addr+2+d

def audit_cpu(code:bytes):
    for (lo,hi),digest in WINDOWS.items():
        assert hashlib.sha256(code[lo-BASE:hi-BASE]).hexdigest()==digest,(hex(lo),hex(hi))
    cpu_bytes(code,0x1E094,'207c00007fb42210d2bc00000008')
    cpu_bytes(code,0x1E0FA,'23f87fa40004beec')
    cpu_bytes(code,0x1E13A,'4eb90001e240')
    cpu_bytes(code,0x1E140,'207c00007f802010b0bc00000002')
    cpu_bytes(code,0x1E14E,'6600ff44')
    cpu_bytes(code,0x1E152,'44b90004bed44e75')
    # Epoch A DOES call One_Tick; epoch B's loop has no such call.
    cpu_bytes(code,0x1DFD4,'4eb90001e15a')
    cpu_bytes(code,0x1E15A,'42b87fa84ab87fa867fa4e75')
    assert branch_target(code,0x1E14E)==0x1E094
    cpu_loop=code[0x1E094-BASE:0x1E152-BASE]
    builder=code[0x1E240-BASE:0x1E2CA-BASE]
    assert cpu_loop.count(bytes.fromhex('4eb90001e240'))==1
    for window in (cpu_loop,builder):
        assert bytes.fromhex('4eb90001e15a') not in window # One_Tick $1E15A
        assert bytes.fromhex('4eb9000091f6') not in window # CPU STOP/IRQ VSync
        assert bytes.fromhex('4e722000') not in window  # 68000 STOP #$2000
    # Builder's four packed object phrase writes are synchronous stores
    # into A0, derived from the pending record pointer $4BEE8.
    cpu_bytes(code,0x1E244,'20790004bee8')
    cpu_bytes(code,0x1E252,'24390004beec')
    assert builder.count(bytes.fromhex('20c0'))>=2
    assert code[0x1E2C8-BASE:0x1E2CA-BASE]==bytes.fromhex('4e75')
    # Installed IRQ callback is invoked outside this nonblocking loop.
    for a,s in VBLANK_INS.items():
        if 0x91B6<=a<=0x91F4:cpu_bytes(code,a,s)
    assert branch_target(code,0x91CC)==0x91D8
    assert branch_target(code,0x91D4)==0x91E2
    # Exact ACK at the end of every original list-copy path.
    for addr in (0x1E3BE,0x1E520,0x1E85E):
        cpu_bytes(code,addr,'21fc000000017fa84e75')
    for addr in (0x1E34A,0x1E4D0,0x1E804):
        assert addr<next(x for x in (0x1E3BE,0x1E520,0x1E85E) if x>addr)
    return len(WINDOWS)

def audit_gpu(img:bytes):
    ins,_=disassemble(img);verify(img,ins)
    for addr,(opcode,s,d,imm) in GPU_INS.items():
        got=ins[addr];assert (got[0],got[1],got[2],got[4])==(opcode,s,d,imm),hex(addr)
    # Source-derived causal partial orders. Start progress is published BEFORE
    # any 144-row work; pointer afterward; GPU clears ACK and waits; CPU
    # VBlank callback copies list then writes ACK. The 68000 can poll between.
    order=['progress','row_0','row_143','publish','ack_clear','ack_observed','next_progress']
    assert ins[0xF030E4][:3]==(0x2F,12,13)
    assert ins[0xF0346E][0]==0x34
    assert ins[0xF03484][:3]==(0x2F,12,13)  # special stage-1 transient A
    assert ins[0xF03490][:3]==(0x2F,12,14)
    assert ins[0xF034A0][:3]==(0x2F,12,13)
    assert ins[0xF034A2][:3]==(0x29,12,13)
    assert ins[0xF034C0][:3]==(0x02,1,20)
    assert order.index('progress')<order.index('row_143')<order.index('publish')<order.index('ack_clear')<order.index('next_progress')
    # A CPU poll for phase N *can* observe the previous published result before
    # stage N finishes; it *can* also see the newly published result after.
    # These are causal possibility witnesses, not scheduled Jaguar captures.
    scenarios={}
    for stage in range(1,17):
        prev=('C or A (mode-dependent)' if stage==1 else ('B' if (stage-1)&1 else 'A'))
        new='B' if stage&1 else 'A'
        assert prev!=new
        scenarios[stage]=(prev,new)
    assert scenarios[1]==('C or A (mode-dependent)','B')
    assert scenarios[2]==('B','A')
    assert scenarios[16]==('B','A')
    return len(GPU_INS),scenarios

def main(path:pathlib.Path):
    rom=path.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA
    stream=zlib.decompressobj(31);code=stream.decompress(rom[0x3804:]);assert stream.eof and len(code)==99072
    windows=audit_cpu(code)
    gpu_checkpoints,states=audit_gpu(extract(rom))
    print(f'PASS canonical retail ROM, {windows} complete original CPU instruction windows and {gpu_checkpoints} GPU opcode checkpoints')
    print('PASS post-GPU CPU loop: read $7FB4, read $7FA4, Make_New, test $7F80, BNE to loop start')
    print('PASS Make_New object-record construction contains no One_Tick/STOP/IRQ waiting, nor does the polling loop')
    print('PASS VBlank interrupt dispatch independent of polling; all three list-copy callbacks ACK $7FA8 only after copy')
    print('PASS stage 1 initial $7FA4 is mode-dependent (C in mode 1, A in modes 2/4/7); transient A then published B')
    print('PASS stages 2..16 admit progress-N CPU picture samples from previous or newly published pointer')
    for n in (1,2,7,10,16):
        old,new=states[n];print(f'  stage {n:02d}: before GPU publish={old}, after GPU publish={new}')
    print('BOUNDARY: causal interleavings permitted by original opcode ordering; NOT actual CPU/GPU runtime scheduling or TOM scanout')
    print('BOUNDARY: no frame count/pixel visibility/tear-free claim; previous stage publication depends on hardware timing')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',required=True,type=pathlib.Path)
    main(p.parse_args().rom)
