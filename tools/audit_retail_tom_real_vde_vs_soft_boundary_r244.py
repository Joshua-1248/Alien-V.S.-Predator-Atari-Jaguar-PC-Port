#!/usr/bin/env python3
"""R244: ROM-authenticated VDE/VI distinction and TOM branch-gated scanout.

Original 68000 SetScreen explicitly programs hardware TOM VDE=$FFFF.
The VI source is instead a separate *software* active bottom saved at $2EE96.
This is read-only original-ROM verification, not a Jaguar bus/cycle emulator.
"""
from __future__ import annotations
import argparse, hashlib, zlib
from pathlib import Path
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_prelist_branch_conditions import build_branch, fields, INSTRUCTIONS as R224
from audit_retail_cross_op_dynamic_chain_r242 import NEW as R242,original_branch,path_for_vc
from audit_retail_cross_irq_inplace_list_r243 import ISR as R243

BASE=0x8000
# All independent starting addresses are boundaries of complete 68000 instructions.
CHECKS={
    0x9022:'303900f14002',      # video type selected by original frontend
    0x902e:'33fc01440002ee8c',  # PAL center = $0144
    0x9062:'33fc010c0002ee8c',  # NTSC center = $010C
    0x90e0:'34390002ee8c',      # D2 = center
    0x90e6:'3602',              # D3 = center
    0x90e8:'9441',              # D2 -= software half-height
    0x90ea:'33c200f00046',      # *actual* TOM VDB = first active line
    0x90f0:'33c20002ee94',      # source/software top border
    0x90f6:'e24a',              # store top/2
    0x90f8:'33c20002ee92',
    0x90fe:'d641',              # D3 += software half-height
    0x9100:'33c30002ee98',      # software bottom border, not TOM VDE
    0x9106:'e24b',              # D3 -> software bottom/2
    0x9108:'33c30002ee96',      # $2EE96: software bottom/2
    0x910e:'33fcffff00f00048',  # *actual TOM VDE* = $FFFF; NOT $2EE98
    0x915c:'6144',              # calls $91A2 VI configuration from SetScreen
    0x9192:'610e',              # another call to the same $91A2 VI configuration
    0x91a2:'30390002ee96',      # D0 = software bottom/2
    0x91a8:'e348',              # D0 * 2
    0x91aa:'d07c0003',          # add three VC half-lines
    0x91ae:'33c000f0004e',      # TOM VI (not VDE)
    0x91b4:'4e75',
    0x921c:'36390002ee94',      # branch first boundary uses software top
    0x922e:'36390002ee98',      # branch second boundary uses software bottom
}

def read_code(rom:Path)->bytes:
    b=rom.read_bytes()
    assert len(b)==4194304 and hashlib.sha256(b).hexdigest()==ROM_SHA,'incorrect original retail ROM'
    z=zlib.decompressobj(31);code=z.decompress(b[0x3804:]);assert z.eof and len(code)==99072
    for addr,hexbytes in {**R224,**R242,**R243,**CHECKS}.items():
        want=bytes.fromhex(hexbytes)
        assert code[addr-BASE:addr-BASE+len(want)]==want,hex(addr)
    assert code.count(bytes.fromhex('33fcffff00f00048'))==1, 'unexpected direct VDE immediate count'
    assert code.count(bytes.fromhex('00f00048'))==1, 'new absolute VDE reference requires review'
    return code

def audit(rom:Path):
    code=read_code(rom)
    # Motorola short BSR displacement is relative to the following instruction.
    for call,callee in [(0x915c,0x91a2),(0x9192,0x91a2)]:
        op=code[call-BASE:call-BASE+2];assert op[0]==0x61 and op[1]<128
        assert call+2+op[1]==callee,(hex(call),hex(callee))
    # Original 68000 immediate decode, not assigned from the software border.
    inst=bytes.fromhex(CHECKS[0x910e]);hw_vde=int.from_bytes(inst[2:4],'big');reg=int.from_bytes(inst[4:8],'big')
    assert reg==0xF00048 and hw_vde==0xffff
    for video,center,height,expected_top,expected_bottom in (
        ('NTSC',0x010c,240,0x001c,0x01fc),
        ('PAL',0x0144,280,0x002c,0x025c)):
        # SetScreen's D1 is height in VC half-lines; software top and bottom.
        top=center-height;bottom=center+height
        assert (top,bottom)==(expected_top,expected_bottom)
        saved_bottom_half=bottom>>1
        # These operations are executed at $9106 and $91A2..$91AE.
        vi=(saved_bottom_half<<1)+3
        assert vi in (0x01ff,0x025f) and vi==bottom+3 and vi&1
        assert vi != (hw_vde+3)&0x7ff,'VI must NOT be derived from TOM VDE'
        assert fields(build_branch(0x7558,top))==(0x7558,top,1)
        assert fields(build_branch(0x7560,bottom))==(0x7560,bottom,1)
        assert fields(original_branch(0x50c00,0,2))==(0x50c00,0,2)
        lines=0;stop_above=0;stop_below=0
        for vc in range(0,0x800,2):
            route=path_for_vc(vc,top,bottom)
            if top<=vc<bottom:
                assert route==('$7548','$7550','$7560','$50C00 staged head')
                lines+=1
            else:
                assert route[-1]=='$7558 STOP'
                if vc<top:stop_above+=1
                else:stop_below+=1
        assert lines==height and stop_above==top//2 and stop_below==1024-bottom//2
        # A direct hardware-VDE-gating hypothesis is contradicted by $FFFF.
        # The branch chain, NOT the VDE register, selects the active picture.
        assert hw_vde>bottom and hw_vde>vi
        assert vi not in range(top,bottom)
        print(f'PASS {video}: TOM VDE=${hw_vde:04X}, software bottom=${bottom:04X}, TOM VI=${vi:04X}; '+
              f'{lines} active branch-routed VC inputs, {stop_above+stop_below} theoretical STOP-routed VC inputs in range 0..$07FE')
    print('PASS original SetScreen writes TOM VDB=$F00046, but TOM VDE=$FFFF at $F00048 (hardware workaround)')
    print('PASS two original callers of VI configuration $91A2; VI sourced from $2EE96, the software display bottom/2')
    print('PASS full authenticated original 68000 instruction checks:',len({**R224,**R242,**R243,**CHECKS}),'unique groups')
    print('CORRECTION: previous "VI = VDE+3" shorthand meant software picture bottom, NOT TOM hardware VDE')
    print('BOUNDARY: static source and logical branch routing only; no bus arbitration, callback runtime, frame display or tearing is asserted')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',required=True,type=Path)
    audit(p.parse_args().rom)
