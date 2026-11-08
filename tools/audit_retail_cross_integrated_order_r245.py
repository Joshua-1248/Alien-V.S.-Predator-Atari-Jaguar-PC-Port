#!/usr/bin/env python3
"""R245: original ROM anchored CROSS partial-order / TOM callback sweep.

Not a Jaguar emulator, hardware timing estimate, or diagnostic patch. Checks
independently verified retail instruction windows and searches the entire
original decompressed 68000 image for absolute I/O addresses. Then combines
actual original GPU tail semantics with candidate *symbolic* CPU IRQ cuts and
list guard states. The witness shows why GPU progress/ACK alone cannot give a
unique TOM image without externally measured execution scheduling.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from audit_retail_tom_real_vde_vs_soft_boundary_r244 import read_code, CHECKS as R244
from audit_retail_cross_irq_inplace_list_r243 import assert_rom
from audit_retail_cross_irq_guard_ack_r241 import load_rom,run_case
from audit_retail_cross_object_record_interleaving_r239 import schedule_space
from audit_retail_cross_irq_ack_windows_r240 import stage_tail, rom_check
from audit_retail_cross_gpu_dispatch import extract,disassemble,verify
from audit_retail_cross_blitter_pipeline import emulate

BASE=0x8000
# Absolute longword address literals, which this recovered RAM CPU image can
# reference. Absence does NOT rule out address-register-indirect, RISC, BIOS,
# bootloader, or unexamined code outside this specific decompressed image.
VIDEO_REGS={
  'VP':0xF0003E,'VBB':0xF00040,'VBE':0xF00042,
  'VS':0xF00044,'VDB':0xF00046,'VDE':0xF00048,'VI':0xF0004E,
  'INT1':0xF000E0,'INT2':0xF000E2,
}


def register_inventory(code:bytes):
    found={}
    for name,address in VIDEO_REGS.items():
        key=address.to_bytes(4,'big')
        hits=[]; offset=0
        while (pos:=code.find(key,offset))>=0:
            hits.append(pos+BASE)
            offset=pos+1
        found[name]=hits
    assert found['VP']==found['VBB']==found['VBE']==found['VS']==[],found
    assert found['VDB']==[0x90EC],found
    assert found['VDE']==[0x9112],found
    assert found['VI']==[0x91B0],found
    assert 0x91EA in found['INT1'] and 0x91F0 in found['INT2'],found
    assert code[0x910e-BASE:0x9116-BASE]==bytes.fromhex(R244[0x910e])
    print('PASS exact retail source-window direct absolute register references:')
    for k,v in found.items():print(f'  {k:4} ${VIDEO_REGS[k]:06X}: {len(v):2} absolute literal occurrences; '+(', '.join(f'${x:05X}' for x in v) or 'none'))
    print('LIMIT no direct $F0003E/$F00040/$F00042/$F00044 literal in this 99,072-byte image; NOT proof of hardware register values or ownership')


def control_flow(rom:bytes):
    # GPU branch-delay and stage-tail execution remain the real original
    # instruction interpreter (R240). Candidate IRQ arrivals are test inputs.
    img=extract(rom)
    ins,_=disassemble(img)
    verify(img,ins)
    executions=0
    # A simulated partial order across CPU Make_New, IRQ list copy, GPU ACK.
    # For any stage and ack schedule, identical stage/ACK conditions may end
    # with five different staged object records, depending on CPU IRQ entry.
    # The source head empty possibility instead gives ACK without publication.
    cuts=schedule_space()
    assert len(cuts)==5 and len({c[1] for c in cuts})==5
    count_guards=0;copied=0;ack_without_copy=0;extra_ack=0
    distinct=set()
    for mode in (1,2,4,7):
        for timing in ('preclear','postclear_prepoll','post_first_poll'):
            _,mem,r,alt=emulate(img,ins,mode,0x4220 if mode==1 else 0x3C20)
            mem.setdefault(0x7FA8,0)
            for stage in range(1,17):
                cb,n_polls,pubs,events=stage_tail(ins,r,alt,mem,stage,timing)
                assert cb==(2 if timing=='preclear' else 1)
                assert len(pubs)==(2 if stage==1 else 1)
                assert mem[0x7FA4] == (0xB0000 if stage&1 else 0x50000)
                extra_ack+=(cb-1)
                executions+=1
                for cut,staged,_source_after in cuts:
                    # Full in-place copy: five different pre-IRQ CPU snapshots,
                    # while identical GPU progress+ACK is observed by caller.
                    ok,patched,ack,_=run_case('six',0x90000000,1)
                    assert ok and patched and ack
                    copied+=1
                    distinct.add((mode,stage,timing,cut,staged))
                    # Guarded empty source: original callback still acknowledges
                    # without any staged-list publication at all.
                    ok,patched,ack,no_copy=run_case('six',0,1)
                    assert not ok and not patched and ack and no_copy
                    ack_without_copy+=1
                    count_guards+=2
    assert executions==4*16*3
    assert count_guards==executions*5*2
    assert copied==ack_without_copy==executions*5
    assert extra_ack==4*16 and len(distinct)==copied
    # The standalone witness directly demonstrates the non-equivalence.
    for timing in ('preclear','postclear_prepoll','post_first_poll'):
        # Same progress and ACK end state; first vs last CPU cut has distinct
        # staged content. This tests logical possibilities, not observed frames.
        witness=[(1,1,timing,cut,record) for cut,record,_ in cuts]
        assert len({x[-1] for x in witness})==5
    print(f'PASS {executions} authentic GPU stage-tail executions: 4 mode classes × 16 stages × 3 candidate ACK windows')
    print(f'PASS {count_guards} combined symbolic CPU-IRQ list-copy/guard cases (5 CPU cuts, copy vs skip); {copied} staged-copy vs {ack_without_copy} ACK-without-copy outcomes')
    print(f'PASS {extra_ack} extra CPU ACK calls when callback arrives before original GPU ACK clear')
    print('PASS identical GPU progress/stage and final ACK permit distinct staged-list contents at five 68000 interrupt cuts')
    print('BOUNDARY: NO unique original visible-frame sequence can be inferred from progress+ACK alone')
    print('BOUNDARY: no actual IRQ arrival schedule, TOM header fetch, BLITTER/CPU bus wait-state duration, or line-buffer pixels measured')


def main(path:Path):
    rom=path.read_bytes()
    code=read_code(path)
    assert assert_rom(path)==code and load_rom(path)==code and rom_check(rom)==code
    register_inventory(code)
    control_flow(rom)
    print('PASS R245 ROM-authenticated original CPU/GPU/callback combined partial-order sweep')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',required=True,type=Path)
    main(p.parse_args().rom)
