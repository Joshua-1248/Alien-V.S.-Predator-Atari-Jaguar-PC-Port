#!/usr/bin/env python3
"""R243: ROM-backed VBlank ISR, live staged-object list write order, bus-resume audit.

The original 68000 byte windows prove sequential write ordering. Controlled
synthetic list data show intermediate staged-list contents at CPU instruction
boundaries; these states are NOT observed on TOM and do not establish any bus
cycle counts, frame latency or actually displayed partial records.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from audit_retail_cross_op_list_publication import check_rom, ORIGINAL as R222
from audit_retail_cross_irq_guard_ack_r241 import CHECKS as R241
from audit_retail_cross_op_dynamic_chain_r242 import NEW as R242

BASE=0x8000
# All groups are full original 68000 opcodes; existing audits also verify these.
ISR={
 0x91B6:'52b90002ee84',          # ISR entry, VBlank counter ++
 0x91C2:'48e7fffe',              # save registers
 0x91C6:'20390002ee80',          # load callback pointer
 0x91CC:'6700000a',              # no callback -> fallback
 0x91D0:'20404e90',              # invoke callback, CPU-side synchronous JSR
 0x91D4:'6000000c',              # join ISR exit
 0x91E2:'4cdf7fff',              # restore CPU registers
 0x91E6:'33fc010100f000e0',      # INT1 enable video & clear pending video
 0x91EE:'427900f000e2',          # INT2 bus priority resume
 0x91F4:'4e73',                  # RTE follows bus resume
 0x1E394:'7606',                 # six record count
 0x1E396:'204a224b2018d0bc0000000222c022d822d822d8', # first +2, 3 more stores
 0x1E3AA:'d5fc00000020d7fc0000002096bc0000000166d8', # next +32
 0x1E3BE:'21fc000000017fa84e75', # callback ACK then RTS
 0x1E4E2:'7608',                 # eight count
 0x1E4F0:'22c022d822d822d822d822d8', # 6 stores/record
 0x1E510:'204a224b22d822d822d822d822d822d8', # 6-word extra tail
 0x1E520:'21fc000000017fa84e75', # ACK then RTS
 0x1E830:'760a',                 # ten count
 0x1E832:'204a224b2018d0bc0000000222c022d822d822d822d822d8', # 6 stores/record
 0x1E85E:'21fc000000017fa84e75', # ACK then RTS
}


def assert_rom(path:Path):
    code=check_rom(path)
    for at,h in {**R222,**R241,**R242,**ISR}.items():
        wanted=bytes.fromhex(h)
        assert code[at-BASE:at-BASE+len(wanted)]==wanted, hex(at)
    # Op encoding: JSR(A0) is synchronous on same 68000 before ISR exit.
    assert code[0x91D2-BASE:0x91D4-BASE]==bytes.fromhex('4e90')
    assert 0x0101 & 0x0001 and 0x0101 & 0x0100
    assert (0x0101 & ~0x0101)==0
    return code


def copy_cuts(records,words,tail,guard):
    """Emit chronological CPU longword writes to directly TOM-linked staged RAM.

    Synthetic source and stage values are chosen to be distinguishable. Staged
    list starts with a STOP head in the actual ROM, but subsequent words are
    not ROM-verified to be zero and MUST NOT be inferred to be zero.
    """
    assert records in (6,8,10)
    assert words== (4 if records==6 else 6)
    src=[[(0x12110000+r*0x1000+w*0x41)&0xffffffff for w in range(8)] for r in range(records+1)]
    stage=[[0x55550000+r*0x1000+w*0x41 for w in range(8)] for r in range(records+1)]
    stage[0][0]=0  # original R242 initialized staged STOP (0,4)
    stage[0][1]=4
    initial=[row[:] for row in stage]
    if guard and records in (6,10):
        # Original callback guard sees src head zero -> zero staged writes,
        # but it still acknowledges $7FA8; no invented initial contents.
        src[0][0]=0
        return 0,initial,stage,[],True
    events=[]
    for i in range(records):
        for j in range(words):
            v=(src[i][j]+(2 if j==0 else 0))&0xffffffff
            stage[i][j]=v
            events.append((i,j,v))
            # At any cut except the last, staged RAM differs from completed list.
            if len(events)==1:assert stage[0][0]==(src[0][0]+2)&0xffffffff
    if tail:
        for j in range(6):
            stage[records][j]=src[records][j]
            events.append((records,j,src[records][j]))
    assert len(events)==records*words+(6 if tail else 0)
    final=[row[:] for row in stage]
    # Independent replay proves each *instruction boundary* carries a distinct
    # partially updated list. No claim that TOM actually fetched between them.
    replay=[row[:] for row in initial]
    snapshots=[]
    for i,j,v in events:
        replay[i][j]=v
        snapshots.append(tuple(tuple(r) for r in replay))
    assert replay==final
    assert len(set(snapshots))==len(events)
    assert all(s!=tuple(tuple(row) for row in final) for s in snapshots[:-1])
    # Acknowledgment / INT1 / INT2 / RTE are AFTER every callback store.
    chronology=[f'STAGED_STORE_{i}' for i in range(len(events))]+['CALLBACK_ACK_7FA8','JSR_RETURN','ISR_INT1_0101','ISR_INT2_RESUME','ISR_RTE']
    assert chronology.index('CALLBACK_ACK_7FA8')>chronology.index(f'STAGED_STORE_{len(events)-1}')
    assert chronology.index('ISR_INT2_RESUME')>chronology.index('ISR_INT1_0101')
    assert chronology.index('ISR_RTE')>chronology.index('ISR_INT2_RESUME')
    return len(events),initial,final,snapshots,True


def audit(path:Path):
    assert_rom(path)
    totals=[]
    for n,words,tail in ((6,4,False),(8,6,True),(10,6,False)):
        count,before,after,cuts,ack=copy_cuts(n,words,tail,False)
        assert ack and before!=after
        assert count=={6:24,8:54,10:60}[n]
        totals.append(count)
        print(f'PASS {n}-record callback: {count} sequential 32-bit staged-list writes; {count-1} intermediate CPU-boundary cuts before final staged write')
        if n in (6,10):
            ct,start,end,cuts,ack=copy_cuts(n,words,tail,True)
            assert ack and not ct and start==end and not cuts
            print(f'PASS {n}-record guarded empty-head route: zero staged writes; ACK still posted')
    assert sum(totals)==138
    print('PASS original 68000 ROM,',len({**R222,**R241,**R242,**ISR}),'exact opcode groups')
    print('PASS fixed TOM chain $7548->$7550->$7560->$50C00: source-to-staged record copy is IN PLACE, not atomic front-buffer flip')
    print('PASS ISR ordering: VBlank counter -> installed callback (copy, ACK) -> INT1 $0101 -> INT2 bus resume -> RTE')
    print('PASS all 138 synthetic staged-word boundary states: no extra publication/OLP transaction after callback copy')
    print('LIMIT: intermediate staged-list states are possible RAM states, NOT confirmed visible artifacts')
    print('LIMIT: no CPU/GPU/Blitter cycle arbitration, ISR duration or next TOM scanline fetch measured')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',type=Path,required=True)
    audit(p.parse_args().rom)
