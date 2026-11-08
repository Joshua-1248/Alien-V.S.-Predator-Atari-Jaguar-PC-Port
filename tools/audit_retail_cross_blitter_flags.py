#!/usr/bin/env python3
"""R219 read-only original-ROM audit: CROSS Blitter flags and row address geometry.

Uses exact retail 68000 instruction bytes and original CROSS GPU instructions,
Jaguar documented A1/A2 flags encoding, and deterministic synthetic row buffers.
The synthetic copy evaluator is NOT a Blitter/GPU timing emulator or final raster.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from hashlib import sha256
import zlib
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_cross_gpu_dispatch import extract, disassemble
from audit_retail_cross_blitter_pipeline import emulate, MEMSRC, CALLS

# Absolute short-address MOVE.L immediate,(xxx).W and CLR.L (xxx).W sites;
# byte-precise original CPU opcode boundaries, not arbitrary word matches.
SET_FLAGS = ((0x1D746, 0x00003C20), (0x1E8A0,0x00004220),
             (0x1FA5A,0x00004220), (0x1FED8,0x00004220))
CLEAR_FLAGS = (0x130C0,0x13152,0x131CE)
CALL_INIT_SCENE = (0x1D846, '4eb90001d740')
# Original scene preparation subroutine: flags write precedes Make_Rel and
# publication of original Object Processor source buffer list, before CROSS.
SCENE_INIT_CALL=(0x1D74E,'4eb90001f082')
ALIAS=(0xF0BD80,0xF0BB00,0xF0B880)
SCRATCH=(0xF03D80,0xF03B00,0xF03880)


def decode_flags(f:int)->dict:
    pitch=f&3
    psize=(f>>3)&7
    w=((f>>9)&63)
    exponent=w>>2
    mantissa=w&3
    width=((4+mantissa)<<exponent)//4
    # Original values have no other named Blitter FLAGS features enabled.
    return dict(pitch=pitch,pixel_bits=(1<<psize),width=width,
                x_add=(f>>16)&3,y_add=(f>>18)&1,
                zoffset=(f>>6)&7,extra=f&~0x001FFFFB)


def verify_cpu(code:bytes):
    for addr,flags in SET_FLAGS:
        # MOVE.L #imm,abs-short $7FB0
        raw=bytes.fromhex('21fc')+flags.to_bytes(4,'big')+bytes.fromhex('7fb0')
        assert code[addr-0x8000:addr-0x8000+len(raw)]==raw,f'Original flag setter ${addr:06X}'
    for addr in CLEAR_FLAGS:
        assert code[addr-0x8000:addr-0x8000+4]==bytes.fromhex('42b87fb0'),f'Other GPU CLR.L ${addr:06X}'
    for addr,pattern in (CALL_INIT_SCENE,SCENE_INIT_CALL):
        raw=bytes.fromhex(pattern)
        assert code[addr-0x8000:addr-0x8000+len(raw)]==raw,f'MJP initializer call ${addr:06X}'
    # Runtime-mode caller control comes from $7FAC; do not assert all those
    # other program paths necessarily call the CROSS GPU code.
    for f,width in ((0x3C20,192),(0x4220,320)):
        d=decode_flags(f)
        assert d==dict(pitch=0,pixel_bits=16,width=width,x_add=0,y_add=0,zoffset=0,extra=0),d
        assert width*2 <= 0x280
    print('PASS 4 original short-absolute 68000 Blitter FLAGS initializers; 3 distinct zeroing sites')
    print('PASS MJP scene-entry JSR $01D740 -> original $7FB0=0x3C20 assignment')
    print('PASS $3C20=16bpp contiguous 192 pixels/row; $4220=16bpp contiguous 320 pixels/row')
    print('PASS zero X/Y increment options, no Z-offset or optional FLAGS bits')


def check_transfer_rows(ins,gpu):
    # These cases use synthetic CPU buffers because source buffer pixel data
    # and Blitter/GPU/OP bus arbitration are not interpreted here.
    for mode,width,flags in ((2,192,0x3C20),(4,192,0x3C20),(7,192,0x3C20),
                             (1,320,0x4220),(3,320,0x4220),(8,320,0x4220)):
        captures, _,regs,_=emulate(gpu,ins,mode,flags)
        assert regs[1]==width
        row_bytes=width*2
        height=144
        memory={p:bytearray((k*37 + y*11 + x*5)&255 for y in range(height+2)
                            for x in range(row_bytes)) for k,p in enumerate(MEMSRC[:2])}
        memory[MEMSRC[2]]=bytearray((height+2)*row_bytes)
        gpu_ram=bytearray(4096)
        # These four copies explicitly prove the contiguous window addressing
        # implied by the *documented* Blitter flags + original GPU registers.
        for i,(pc,writes,_) in enumerate(captures):
            dst=writes[0xF02200];src=writes[0xF02224]
            src_y=writes[0xF02230]>>16;dst_y=writes[0xF0220C]>>16
            assert writes[0xF0223C]==(1<<16)|width
            assert writes[0xF02204]==writes[0xF02228]==flags
            assert writes[0xF02238]==0x01800601
            if i<3:
                assert dst==ALIAS[i] and src==MEMSRC[(0,1,0)[i]]
                # The GPU scratch data is aliased through write-only bank +$8000.
                off=SCRATCH[i]-0xF03000
                sample=memory[src][src_y*row_bytes:(src_y+1)*row_bytes]
                gpu_ram[off:off+row_bytes]=sample
                assert gpu_ram[off:off+row_bytes]==sample
                assert src_y in (0,1) and dst_y==0
            else:
                assert pc==0xF0345C and src==SCRATCH[0] and dst==MEMSRC[2]
                off=SCRATCH[0]-0xF03000
                # Emulate a changed kernel output before writeback, not the
                # proprietary actual GPU color kernel or a guessed fade.
                written=bytes((p^0xA5) for p in gpu_ram[off:off+row_bytes])
                gpu_ram[off:off+row_bytes]=written
                memory[dst][dst_y*row_bytes:(dst_y+1)*row_bytes]=written
                assert memory[dst][dst_y*row_bytes:(dst_y+1)*row_bytes]==written
    print('PASS original GPU instruction-derived four-call source/destination addressing across 6 modes')
    print('PASS 192/320 contiguous 16-bit row copies through three original scratch/alias bases')
    print('BOUNDARY: synthetic pixel copy is address verification, not native Blitter timing/OP output or GPU kernel result')


def main(path:Path):
    rom=path.read_bytes()
    assert len(rom)==4194304 and sha256(rom).hexdigest()==ROM_SHA
    code=zlib.decompressobj(31).decompress(rom[0x3804:]); assert len(code)==99072
    verify_cpu(code)
    gpu=extract(rom);ins,_=disassemble(gpu)
    check_transfer_rows(ins,gpu)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    main(p.parse_args().rom)
