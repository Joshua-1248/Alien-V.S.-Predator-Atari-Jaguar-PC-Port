#!/usr/bin/env python3
"""R224: original AVP Jaguar PRE_LIST branch phrase / scanline gating audit.

Authenticates the original retail 68000 instructions and derives the two
BRANCHOBJ phrases by executing the exact documented integer transformations.
Checks routing for NTSC/PAL MJP geometry at every even VC half-line.

NO cycle-level TOM model, video capture, commercial ROM output, code patches,
or approximation of GPU/Object Processor bus arbitration.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import zlib

ROM_SHA = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
BASE=0x8000
# Separately keyed complete instruction groups; no overlapping-key overwrites.
INSTRUCTIONS = {
    0x90E0:'34390002ee8c',        # MOVE.W $2EE8C,D2 (video vertical centre)
    0x90E6:'3602',                # D3 = D2
    0x90E8:'9441',                # D2 -= height (D1)
    0x90EA:'33c200f00046',        # Set VDB
    0x90F0:'33c20002ee94',        # Save first branch boundary
    0x90F6:'e24a',                # D2 >>= 1
    0x90F8:'33c20002ee92',        # Save top half-line
    0x90FE:'d641',                # D3 += height (D1)
    0x9100:'33c30002ee98',        # Save second branch boundary
    0x9106:'e24b',                # D3 >>= 1
    0x9108:'33c30002ee96',        # Save bottom half-line
    0x9210:'207c00007548',        # PRE_LIST A0
    0x9216:'2248d2fc0010',        # A1 = $7558
    0x921C:'36390002ee94',        # D3 = VDB
    0x9222:'123c0001',            # D1=1 (branch CC)
    0x9226:'6152',                # BSR $927A
    0x9228:'227c00007560',        # A1=$7560
    0x922E:'36390002ee98',        # D3=VDE
    0x9234:'123c0001',            # CC1
    0x9238:'6140',                # BSR $927A
    0x923A:'610000cc',            # BSR $9308
    0x923E:'4e75',                # RTS
    0x927A:'2409',                # D2 = A1
    0x927C:'4842',                # SWAP D2
    0x927E:'eb9a',                # ROL.L #5,D2
    0x9280:'4280',                # CLR.L D0
    0x9282:'3002',                # MOVE.W D2,D0
    0x9284:'20c0',                # write first long
    0x9286:'4280',                # CLR.L D0
    0x9288:'1001',                # MOVE.B D1,D0
    0x928A:'4840',                # SWAP D0
    0x928C:'e488',                # LSR.L #2,D0
    0x928E:'4242',                # CLR.W D2
    0x9290:'8082',                # OR.L D2,D0
    0x9292:'e74b',                # LSL.W #3,D3
    0x9294:'8043',                # OR.W D3,D0
    0x9296:'807c0003',            # OR.W #3,D0 (BRANCHOBJ type)
    0x929A:'20c0',                # write second long
    0x929C:'4e75',                # RTS
    0x9308:'4298',                # CLR.L (A0)+ (STOP phrase)
    0x930A:'20fc00000004',        # MOVE.L #4,(A0)+
    0x9310:'23c80002eeb4',        # save cursor
    0x9316:'42b90002eeb8',        # reset object count
    0x931C:'4e75',                # RTS
}

def retail_code(path:Path)->bytes:
    rom=path.read_bytes()
    if len(rom)!=4194304 or hashlib.sha256(rom).hexdigest()!=ROM_SHA:
        raise AssertionError('canonical retail AVP.jag required')
    z=zlib.decompressobj(31)
    code=z.decompress(rom[0x3804:])
    if not z.eof or len(code)!=99072:
        raise AssertionError('retail compressed 68000 stream mismatch')
    for addr,h in INSTRUCTIONS.items():
        want=bytes.fromhex(h)
        actual=code[addr-BASE:addr-BASE+len(want)]
        if actual!=want:raise AssertionError(f'68000 opcode mismatch ${addr:06X}')
    for at,dest in ((0x9226,0x927A),(0x9238,0x927A),(0x923A,0x9308)):
        raw=int.from_bytes(code[at-BASE:at-BASE+2],'big')
        if at==0x923A:
            assert raw==0x6100
            disp=int.from_bytes(code[at-BASE+2:at-BASE+4], 'big',signed=True)
            assert at+2+disp==dest
        else:
            assert raw>>8==0x61
            disp=raw&255
            if disp>127:disp-=256
            assert at+2+disp==dest
    return code

def rol32(x:int,n:int)->int:
    return ((x<<n)|(x>>(32-n)))&0xffffffff

def build_branch(link:int,ypos:int,cc:int=1)->tuple[int,int]:
    """Execute retail $927A-$929A 68000 word packing (not hardcoded phrases)."""
    assert link%8==0 and 0<=link<0x400000 and 0<=ypos<=0x7ff and cc==1
    d2=rol32(((link&0xffff)<<16)|(link>>16),5)  # MOVE,SWAP,ROL #5
    first=d2&0xffff                         # CLR.L; MOVE.W D2,D0
    d0=(cc<<16)>>2                        # MOVE.B,SWAP,LSR.L #2
    d2&=0xffff0000                        # CLR.W D2
    d0 |= d2                              # OR.L D2,D0
    d3=(ypos<<3)&0xffff                   # LSL.W #3,D3
    d0|=d3                                # OR.W D3,D0
    d0|=3                                 # OR.W #3,D0
    return first,d0

def fields(words:tuple[int,int])->tuple[int,int,int]:
    phrase=(words[0]<<32)|words[1]
    return ((phrase>>24)&0x7ffff)<<3, (phrase>>3)&0x7ff, (phrase>>14)&7

def route(vc:int, top:int,bottom:int)->str:
    # Branch 1: if YPOS > VC, branch to STOP at $7558.
    # Branch 2: if YPOS > VC, branch to dynamic list at $7560.
    if top>vc:return 'stop'
    if bottom>vc:return 'list'
    return 'stop'

def audit(path:Path)->None:
    retail_code(path)
    print(f'PASS retail ROM identity and {len(INSTRUCTIONS)} exact nonoverlapping 68000 instruction groups')
    for label,center,height in [('NTSC 320x240',0x10c,240),('PAL 320x280',0x144,280)]:
        top=(center-height)&0xffff
        bottom=(center+height)&0xffff
        bra0=build_branch(0x7558,top)
        bra1=build_branch(0x7560,bottom)
        assert fields(bra0)==(0x7558,top,1),fields(bra0)
        assert fields(bra1)==(0x7560,bottom,1),fields(bra1)
        assert bra0 == (0x0000000e, (0xab<<24)|(1<<14)|(top<<3)|3)
        assert bra1 == (0x0000000e, (0xac<<24)|(1<<14)|(bottom<<3)|3)
        rows=0
        for vc in range(0,0x800,2):
            actual=route(vc,top,bottom)
            expected='list' if top<=vc<bottom else 'stop'
            assert actual==expected,(label,vc,actual,expected)
            rows += actual=='list'
        assert rows==height,(label, rows, height)
        # Just above, on first active, final active and at VDE.
        assert [route(v,top,bottom) for v in (top-2,top,bottom-2,bottom)]==['stop','list','list','stop']
        print(f'PASS {label}: VDB=${top:04X} VDE=${bottom:04X}; '
              f'branch STOP $7558 / active list $7560; {rows} active scanlines')
        print(f'  verified BRANCHOBJ phrase words: '
              f'${bra0[0]:08X} ${bra0[1]:08X}; ${bra1[0]:08X} ${bra1[1]:08X}')
    print('PASS 68000 STOP builder at $9308 writes two longwords (0,4) after branches')
    print('LIMIT: logical VC/BRANCHOBJ routing only; scanline bus timing, callbacks, GPU/OP synchronization and displayed pixels not simulated')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True)
    audit(p.parse_args().rom)
