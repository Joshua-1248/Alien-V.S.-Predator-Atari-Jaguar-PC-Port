#!/usr/bin/env python3
"""R232 ROM-only DSP startup and two interrupt-vector identity checks.

Tests exact 68000 instructions and original FullSynth DSP instruction words,
then resolves the MOVEI/JUMP destinations used by the hardware-vector stubs.
This is NOT an emulator, proof that interrupts fire, or an audio timing test.
Never writes commercial ROM or DSP bytes to an output artifact.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from audit_retail_dsp_master_gain_r230 import decode_original,word,fields,DSP_BASE,CPU_BASE


def checkcpu(cpu:bytes,pc:int,hexbytes:str):
    want=bytes.fromhex(hexbytes)
    got=cpu[pc-CPU_BASE:pc-CPU_BASE+len(want)]
    assert got==want,(hex(pc),got.hex(),want.hex())


def movi(img:bytes,pc:int,reg:int)->int:
    assert fields(word(img,pc))==(38,0,reg), (hex(pc), fields(word(img,pc)))
    return word(img,pc+2) | (word(img,pc+4)<<16)


def check_vector(img:bytes,offset:int,target:int):
    vec=DSP_BASE+offset
    assert movi(img,vec,29)==target
    # DSP JUMP (r29), unconditional, with hardware delay slot.
    assert fields(word(img,vec+6))==(52,29,0)
    assert fields(word(img,vec+8))==(57,0,0)
    assert target>=DSP_BASE and target<DSP_BASE+len(img)
    assert target%2==0
    return vec,target


def audit(cpu:bytes,img:bytes):
    # FullSynth image loaded at F1B000, followed by DSP PC and DSPGO writes.
    checkcpu(cpu,0x9868,'23fc00f1b00000f1a110')
    checkcpu(cpu,0x9872,'23fc0000000100f1a114')
    # The same CPU ROM has explicit DSP stop/reset writes during earlier setup.
    checkcpu(cpu,0x97CC,'23fc0000000000f1a114')
    assert movi(img,DSP_BASE,0)==0xF1B034
    assert fields(word(img,DSP_BASE+6))==(52,0,0)
    # Hardware vector 1 at local-RAM +$10: serial/I2S. Vector 2 at
    # +$20: Timer 1. ROM stubs each jump via r29 to separate DSP routines.
    audio=check_vector(img,0x10,0xF1B596)
    timer=check_vector(img,0x20,0xF1B5EE)
    assert audio[1]!=timer[1]
    # The audio routine reads paired words from FullSynth ring memory, and
    # sends them to TX base and TX base+4; earlier R231 audited full operands.
    assert movi(img,0xF1B59E,29)==0xF1BA10
    for pc,expected in (
        (0xF1B5A8,(41,29,30)),(0xF1B5AA,(47,28,30)),
        (0xF1B5B0,(41,29,30)),(0xF1B5B2,(47,28,30)),
    ):
        assert fields(word(img,pc))==expected,(hex(pc),fields(word(img,pc)))
    # The timer-1 handler is a *separate* original instruction block. Do
    # not infer original PIT programming or its active IRQ frequency here.
    assert fields(word(img,0xF1B5EE))==(34,20,30)
    assert fields(word(img,0xF1B5F0))==(41,30,22)
    print('PASS verified retail ROM, CPU $9868 DSP PC=$F1B000 and $9872 DSPGO=1')
    print('PASS original DSP boot vector +$00 branches to $F1B034')
    print('PASS original DSP serial vector +$10 branches to $F1B596')
    print('PASS original DSP Timer-1 vector +$20 branches to $F1B5EE')
    print('PASS source instruction order at serial handler and paired ring-to-TX writes')
    print('LIMIT: static vector destinations, not live IRQ cadence, DSP timing or exact audible output')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    audit(*decode_original(p.parse_args().rom))
