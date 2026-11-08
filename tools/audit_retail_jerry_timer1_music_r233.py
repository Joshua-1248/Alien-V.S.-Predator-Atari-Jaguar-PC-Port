#!/usr/bin/env python3
"""R233 retail-ROM executable proof of the JERRY Timer-1 -> FullSynth music path.

Original 68000 + DSP instruction identities, CPU timer address census, host
source contract. Not a live Jaguar interrupt scheduler or a sound comparison.
Never exports commercial ROM, audio, instructions, or source payloads.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import struct
from audit_retail_dsp_master_gain_r230 import decode_original,CPU_BASE,DSP_BASE,word,fields


def exact(cpu: bytes, at: int, seq: str):
    want=bytes.fromhex(seq)
    got=cpu[at-CPU_BASE:at-CPU_BASE+len(want)]
    assert got==want,(hex(at),got.hex(),want.hex())


def check_movei(img:bytes,at:int,reg:int,value:int):
    assert fields(word(img,at))==(38,0,reg),(hex(at),fields(word(img,at)))
    assert (word(img,at+2)|(word(img,at+4)<<16))==value


def audit(cpu:bytes,img:bytes,source:Path):
    # Original sound-engine resets Timer 1 while bringing up JERRY,
    # and again at music stop/replace. Both use 16-bit immediate stores.
    reset='33fc000000f1000033fc000000f10002'
    exact(cpu,0x97a0,reset)
    exact(cpu,0x98ba,reset)
    # Original score descriptor words at offsets +$10 and +$12 go into
    # timer DIVIDER $F10002 then PRESCALER $F10000, respectively.
    exact(cpu,0x9a26,'33e8001000f10002')
    exact(cpu,0x9a2e,'33e8001200f10000')
    # The immediately preceding original call computes/updates tempo
    # bookkeeping before changing the two physical timer registers.
    exact(cpu,0x9a1c,'3028000c4eb900009a38')
    exact(cpu,0x9a36,'4e75')
    # All raw long address literals in the original 68000 image are
    # accounted for, including one in a non-executable data table.
    census={
        0xF10000:[0x853a,0x97a4,0x98be,0x9a32],
        0xF10002:[0x97ac,0x98c6,0x9a2a],
    }
    for addr,expected in census.items():
        target=struct.pack('>I',addr)
        observed=[CPU_BASE+k for k in range(0,len(cpu)-3,2) if cpu[k:k+4]==target]
        assert observed==expected,(hex(addr),observed,expected)
    # The original DSP also has an explicit Timer-1 rearm sequence.
    check_movei(img,0xF1B5D2,2,0xF10000)
    check_movei(img,0xF1B5D8,3,0xF10002)
    assert fields(word(img,0xF1B5DE))==(35,0,1) # MOVEQ #1,R1
    assert fields(word(img,0xF1B5E0))==(46,2,1) # STOREW R1,(R2)
    assert fields(word(img,0xF1B5E2))==(46,3,1) # STOREW R1,(R3)
    # Timer1 ISR is distinct from I2S ISR and references the original
    # DSP-local sequencer state table. Do not confuse with sample output.
    assert fields(word(img,0xF1B5EE))==(34,20,30)
    assert fields(word(img,0xF1B5F0))==(41,30,22)
    check_movei(img,0xF1B60C,23,0xF1B98C)
    check_movei(img,0xF1B622,8,0xF1BA84)
    check_movei(img,0xF1B62E,23,0xF1BAC4)
    check_movei(img,0xF1B636,8,0xF1BAFC)
    check_movei(img,0xF1B642,23,0xF1BB3C)
    # The host scheduler uses the original descriptor field orientation:
    # timer divider from +16 and prescaler from +18, both +1 divisions.
    host=(source/'src/jaguar/host/fullsynth_host.c').read_text()
    assert 'div=(u16)((d[16]<<8)|d[17])' in host
    assert 'pre=(u16)((d[18]<<8)|d[19])' in host
    assert '(pre+1u)*(uint64_t)(div+1u)*20774ull' in host
    assert 'F.music.pit_accum+=26590906ull' in host
    # Example descriptor settings exercise positive divisor domain without
    # passing off arithmetic examples as observed retail song descriptors.
    for pre,div in [(0,0),(0,1),(1,0),(4,127),(255,255),(65535,65535)]:
        ticks=(pre+1)*(div+1)
        assert ticks>0 and ticks<=2**32
    print('PASS original retail ROM/68000/DSP identity from R230 canonical SHA-256')
    print('PASS 2 Timer-1 reset pairs and $9A26/$9A2E score-descriptor timer writes')
    print('PASS complete raw 68000 timer address census, separating executable vs data')
    print('PASS DSP Timer1 rearm, separate Timer1 handler and original sequencer table addresses')
    print('PASS host descriptor field order and tempo-divider +1 arithmetic source contract')
    print('LIMIT: no verified Jaguar PIT interrupt cycles, timer phase, tempo waveform or sample-exact output')

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',type=Path,required=True)
    ap.add_argument('--source',type=Path,default=Path(__file__).resolve().parent.parent)
    a=ap.parse_args()
    audit(*decode_original(a.rom),a.source)
