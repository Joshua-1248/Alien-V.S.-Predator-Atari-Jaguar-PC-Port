#!/usr/bin/env python3
"""R240: original CROSS GPU/VBlank acknowledgment window audit.

ROM-authenticated Jaguar GPU RISC stage-tail execution with three *synthetic*
CPU callback arrival schedules. A source-validated ISR writes the VBlank ACK
only after its list copy. This proves the GPU's clear/poll semantics and
illustrates non-unique callback counts, not hardware cycle timing or scanout.
No production code or copyrighted ROM bytes are emitted.
"""
from __future__ import annotations
import argparse, pathlib, hashlib, zlib
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_cross_gpu_dispatch import extract, disassemble, verify
from audit_retail_cross_gpu_publication import GPU as CHECKPOINTS
from audit_retail_cross_blitter_pipeline import emulate
from audit_retail_cross_cpu_busy_poll_r238 import WINDOWS

MASK=0xffffffff
BASE=0x8000
# R218/R237 symbolic GPU input pointers (never presented as real physical allocations).
A=0x50000; B=0xB0000; C=0x70000; D=0x90000
ACK=0x7FA8; PUB=0x7FA4
CPU_CHECKS={
    0x91B6:'52b90002ee84',       # VBlank frame counter increment
    0x91C6:'20390002ee80',       # CPU calls installed callback
    0x91D0:'20404e90',
    0x91E6:'33fc010100f000e0',   # acknowledge TOM interrupt
    0x91EE:'427900f000e2',       # restore interrupted bus priorities (INT2)
    0x91F4:'4e73',               # RTE
    0x1E3BE:'21fc000000017fa84e75', # ACK last in six-record callback
    0x1E520:'21fc000000017fa84e75', # ACK last in eight-record callback
    0x1E85E:'21fc000000017fa84e75', # ACK last in ten-record callback
    0x1E15A:'42b87fa84ab87fa867fa4e75', # distinct pre-GPU One_Tick wait
}


def rom_check(rom:bytes):
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA
    zs=zlib.decompressobj(31); code=zs.decompress(rom[0x3804:])
    assert zs.eof and len(code)==99072
    for (lo,hi),digest in WINDOWS.items():
        assert hashlib.sha256(code[lo-BASE:hi-BASE]).hexdigest()==digest,hex(lo)
    for addr,expected in CPU_CHECKS.items():
        b=bytes.fromhex(expected)
        assert code[addr-BASE:addr-BASE+len(b)]==b,hex(addr)
    return code


def stage_tail(ins,r,alt,mem,stage,schedule):
    """Execute real stage-tail opcodes and delay slots; only CPU IRQ timing
    (and thus externally written ACK) is synthetic, explicitly recorded.
    """
    assert schedule in ('preclear','postclear_prepoll','post_first_poll')
    pc=0xF03472;eq=False;steps=0;polls=0;callbacks=0
    events=[];publications=[]
    def cb(when):
        nonlocal callbacks
        # Models only the *last* ISR callback store; the original callback
        # copy itself is separately verified by R222/R239. The actual callback
        # must be one of the ACK-producing routines with guards permitting it.
        assert mem[ACK] in (0,1)
        mem[ACK]=1
        callbacks+=1
        events.append(('cpu_cb_ack',when))
    def execute(at):
        nonlocal eq,polls
        op,s,d,span,imm=ins[at]
        if op==0x25:r[d]=alt[s]
        elif op==0x26:r[d]=imm
        elif op==0x23:r[d]=s
        elif op==0x22:r[d]=r[s]
        elif op==0x04:r[d]=(r[d]-r[s])&MASK
        elif op==0x00:r[d]=(r[d]+r[s])&MASK
        elif op==0x02:r[d]=(r[d]+(s or 32))&MASK
        elif op==0x1F:eq=(r[d]==s)
        elif op==0x1E:eq=(r[d]==r[s])
        elif op==0x29:
            address=r[s]
            if at==0xF034A2:
                polls+=1
                if schedule=='postclear_prepoll' and polls==1:cb('after clear, before first GPU load')
                if schedule=='post_first_poll' and polls==2:cb('after first empty poll')
                if schedule=='preclear' and polls==2:cb('second callback after first poll')
            assert address in mem,(hex(at),hex(address))
            r[d]=mem[address]
            if at==0xF034A2:events.append(('gpu_load_ack',r[d]))
        elif op==0x2F:
            address=r[s];mem[address]=r[d]
            if at in (0xF03484,0xF03490):
                assert address==PUB
                publications.append((at,r[d]))
                events.append(('gpu_publish',at,r[d]))
            if at==0xF034A0:
                assert address==ACK and r[d]==0
                events.append(('gpu_clear_ack',0))
        elif op==0x39:pass
        else:raise AssertionError((hex(at),op))
    while pc not in (0xF030DA,0xF034CC):
        steps+=1
        assert steps<110,(stage,schedule,events)
        if pc==0xF034A0 and schedule=='preclear':cb('after GPU publication, before clear')
        op,s,d,span,imm=ins[pc]
        nextpc=pc+span
        if op in (0x34,0x35):
            target=(r[s] if op==0x34 else nextpc+(s if s<16 else s-32)*2)
            taken=d==0 or (d==1 and not eq) or (d==2 and eq)
            execute(nextpc) # Jaguar GPU branch delay slot
            nextpc=target if taken else nextpc+ins[nextpc][3]
        else:execute(pc)
        pc=nextpc
    assert pc==(0xF034CC if stage==16 else 0xF030DA)
    assert mem[ACK]==1
    assert mem[PUB]==(B if stage&1 else A)
    assert r[3]==(A if stage&1 else B)
    assert publications==([(0xF03484,A),(0xF03490,B)] if stage==1 else [(0xF03490,B if stage&1 else A)])
    assert callbacks==(2 if schedule=='preclear' else 1)
    assert polls==(1 if schedule=='postclear_prepoll' else 2)
    assert events.index(('gpu_clear_ack',0)) < next(i for i,e in enumerate(events) if e[0]=='gpu_load_ack')
    # A callback before the GPU clears ACK is overwritten; only the second
    # ACK can release this stage. Both paths are allowed by instruction order.
    if schedule=='preclear':
        assert events.index(('cpu_cb_ack','after GPU publication, before clear'))<events.index(('gpu_clear_ack',0))
    return callbacks,polls,publications,events


def main(path:pathlib.Path):
    rom=path.read_bytes();rom_check(rom)
    img=extract(rom);ins,_=disassemble(img);verify(img,ins)
    for at,(op,s,d,imm) in CHECKPOINTS.items():
        got=ins[at];assert (got[0],got[1],got[2],got[4])==(op,s,d,imm),hex(at)
    counts={x:0 for x in ('preclear','postclear_prepoll','post_first_poll')}
    for mode in (1,2,4,7):
        for schedule in counts:
            _,mem,r,alt=emulate(img,ins,mode,0x4220 if mode==1 else 0x3C20)
            mem.setdefault(ACK,0)
            assert mem[PUB]==(C if mode==1 else A)
            assert (alt[1],alt[2],r[3])==(A,B,B)
            for stage in range(1,17):
                # The original stage entry publishes progress before row work;
                # the R220 full-row instruction audit separately covers rows.
                assert ins[0xF030E4][:3]==(0x2F,12,13)
                r[20]=stage;r[12]=0x7FB4;r[13]=stage
                mem[0x7FB4]=stage
                # Real $F03472 tail follows all 144 rows.
                cb,polls,pubs,ev=stage_tail(ins,r,alt,mem,stage,schedule)
                assert cb in (1,2) and polls in (1,2)
                counts[schedule]+=1
                assert [e[2] for e in ev if e[0]=='gpu_publish'] == ([A,B] if stage==1 else [B if stage&1 else A])
    assert counts=={'preclear':64,'postclear_prepoll':64,'post_first_poll':64},counts
    # The ISR is one CPU: it invokes the selected callback then writes
    # TOM INT1/INT2 and executes RTE; this is *not* a simultaneous CPU copy.
    print('PASS verified retail ROM,',len(WINDOWS),'68000 code windows,',len(CPU_CHECKS),'targeted ISR/ACK groups,',len(CHECKPOINTS),'GPU instruction checkpoints')
    print('PASS 4 modes x 16 stages x 3 synthetic IRQ-ACK windows =',sum(counts.values()),'original-instruction GPU tail executions')
    print('PASS early ACK overwritten by original GPU clear: requires a subsequent callback before progress can advance')
    print('PASS callback after clear but before first poll: immediate next load sees ACK; 1 callback / 1 poll')
    print('PASS callback after first empty poll: 1 callback / 2 polls')
    print('PASS 16 alternating output publications and stage-1 special double-publication in every schedule')
    print('PASS IRQ callback then TOM interrupt acknowledgment + INT2 resume + CPU RTE original instruction order')
    print('BOUNDARY: candidate callback times are NOT observed actual Jaguar IRQs; callback guard, bus latency, TOM line-buffer fetch not simulated')
    print('BOUNDARY: this does NOT establish number of original VBlanks or displayed frames per GPU stage')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',type=pathlib.Path,required=True)
    main(p.parse_args().rom)
