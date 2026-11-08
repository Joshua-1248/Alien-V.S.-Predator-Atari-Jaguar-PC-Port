#!/usr/bin/env python3
"""R231 read-only retail-ROM audit: CPU serial-clock setup through DSP buffered stereo output.

This verifies original instruction bytes and address/data dependencies; it does
NOT emulate JERRY interrupt latency, waveform synthesis, DAC, or PCM timing.
Commercial ROM/DSP code is never saved to disk or included with releases.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from audit_retail_dsp_master_gain_r230 import decode_original, word, fields, DSP_BASE, CPU_BASE


def cpu_instruction(code:bytes,pc:int,data:str)->None:
    expect=bytes.fromhex(data)
    got=code[pc-CPU_BASE:pc-CPU_BASE+len(expect)]
    assert got==expect, f'68000 mismatch at ${pc:06X}: {got.hex()} expected {expect.hex()}'


def dsp_instruction(img:bytes,pc:int,opcode:int,source:int,dest:int)->None:
    got=fields(word(img,pc))
    assert got==(opcode,source,dest),f'DSP opcode mismatch at ${pc:06X}: {got}'


def dsp_movei(img:bytes,pc:int,reg:int,value:int)->None:
    dsp_instruction(img,pc,0x26,0,reg)
    actual=word(img,pc+2)|(word(img,pc+4)<<16)
    assert actual==value, f'DSP MOVEI mismatch at ${pc:06X}: ${actual:08X}'


def audit(cpu:bytes,img:bytes)->None:
    # Retail CPU startup: program SCLK divider N=0x13 and serial mode 0x15.
    cpu_instruction(cpu,0x009840,'23fc0000001300f1a150')
    cpu_instruction(cpu,0x00984A,'23fc0000001500f1a154')
    # Prior reset explicitly disables the serial interface (zero written to both).
    cpu_instruction(cpu,0x0097B0,'23fc0000000000f1a154')
    cpu_instruction(cpu,0x0097BA,'23fc0000000000f1a150')

    # Original FullSynth init seeds the transmit-port base address in the
    # alternate register bank, later used by its sample-output path.
    dsp_movei(img,0xF1B072,0,0xF1A148)      # serial transmit left base
    dsp_instruction(img,0xF1B078,0x24,0,9) # MOVETA r0,r9

    # Original mixing block writes one stereo pair (r26, r9) to two
    # adjacent 32-bit entries in its DSP-local output ring, F1BA10 + offset.
    dsp_movei(img,0xF1B114,1,0xF1BA10)
    dsp_instruction(img,0xF1B120,0x2F,1,26) # STORE (r1),r26
    dsp_instruction(img,0xF1B122,0x02,4,1)  # ADDQ #4,r1
    dsp_instruction(img,0xF1B124,0x2F,1,9)  # STORE (r1),r9

    # Separate service routine reads those buffered sample words before
    # storing them into the two successive serial transmit registers.
    dsp_movei(img,0xF1B59E,29,0xF1BA10)
    dsp_instruction(img,0xF1B5A4,0x00,28,29) # ADD r28,r29
    dsp_instruction(img,0xF1B5A6,0x22,9,28)  # MOVE r9,r28 (tx base)
    dsp_instruction(img,0xF1B5A8,0x29,29,30) # LOAD (r29),r30
    dsp_instruction(img,0xF1B5AA,0x2F,28,30) # STORE (r28),r30
    dsp_instruction(img,0xF1B5AC,0x02,4,29)  # ring pointer +4
    dsp_instruction(img,0xF1B5AE,0x02,4,28)  # transmit port +4
    dsp_instruction(img,0xF1B5B0,0x29,29,30) # LOAD next channel
    dsp_instruction(img,0xF1B5B2,0x2F,28,30) # STORE next channel

    # Primary DSP sample producer's master gain is already R230-verified.
    dsp_movei(img,0xF1B104,2,0xF1BB40)
    dsp_instruction(img,0xF1B10A,0x29,2,2)
    dsp_instruction(img,0xF1B10C,0x11,2,9)
    dsp_instruction(img,0xF1B10E,0x11,2,26)

    # Jaguar serial output: SCLK = system clock/(2*(N+1)); stereo strobe
    # alternates each 16 serial clocks = 32 clocks/pair. Source-referenced
    # host system-clock 26,590,906 Hz, not a precise retail-clock capture.
    cpu_sclk_divider=0x13
    host_system_clock=26590906
    expected_hz=host_system_clock/(2*(cpu_sclk_divider+1)*32)
    assert 20774 < expected_hz < 20775
    assert (0x15 & 1) and (0x15 & 4) and (0x15 & 16)
    assert not (0x15 & 0x20)

    print('PASS retail 68000 serial interface disable, SCLK=$13, SMODE=$15 source instructions')
    print('PASS original DSP alternate-register TX base $F1A148 and F1BA10 ring producer')
    print('PASS original DSP two consecutive channel LOAD/STORE pairs at TX base and TX base+4')
    print('PASS same original DSP common master-volume load and stereo multiply')
    print('DERIVED serial stereo rate assuming 26,590,906 Hz host crystal value: %.9f Hz' % expected_hz)
    print('LIMIT CPU control and DSP instruction-level dependencies only; real interrupt scheduling, DAC-level channels, PCM and precise clock not measured')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    audit(*decode_original(p.parse_args().rom))
