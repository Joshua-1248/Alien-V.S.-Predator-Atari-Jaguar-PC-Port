#!/usr/bin/env python3
"""R242 exact ROM-derived three-branch TOM routing / staging initialization audit.

Reconstructs the original 68000 branch-object writer at $927A, the MJP
$1BE78..$1BEB8 initialization of both STOP heads and the dynamic branch,
then verifies logical PRE_LIST->staged-list traversal across every active
NTSC/PAL half-line. No clocks, hardware captures, or original bytes emitted.
"""
from __future__ import annotations
import argparse,hashlib,zlib
from pathlib import Path
from audit_retail_prelist_branch_conditions import INSTRUCTIONS as R224,build_branch as r224_branch,fields
from audit_retail_cross_op_list_publication import ORIGINAL as R222
from audit_retail_mjp_gpu_handshake import ROM_SHA

BASE=0x8000
# Full exact original opcode windows; starts are aligned to 68000 instructions.
NEW={
  0x1BE78:'23fc000000000004fc00',  # source head first longword STOP zero
  0x1BE82:'23fc000000040004fc04',  # source head second longword STOP four
  0x1BE8C:'23fc0000000000050c00',  # staging head first longword STOP zero
  0x1BE96:'23fc0000000400050c04',  # staging head second longword STOP four
  0x1BEA0:'227c00050c00',          # A1 = $50C00 (staged list)
  0x1BEA6:'207c00007560',          # A0 = $7560 (dynamic branch in PRE_LIST)
  0x1BEAC:'4243',                  # CLR.W D3 -> YPOS zero
  0x1BEAE:'123c0002',              # D1=2 -> BRANCHOBJ CC=2
  0x1BEB2:'4eb90000927a',          # invoke original branch phrase packer
  0x9192:'610e',                  # setup BSR.S $91A2 sets vertical IRQ line
  0x91A2:'30390002ee96',          # D0 = saved VDE half count
  0x91A8:'e348',                  # LSL.W #1, D0 -> VDE VC units
  0x91AA:'d07c0003',              # ADD.W #3,D0 -> odd VI after VDE
  0x91AE:'33c000f0004e',          # MOVE.W D0,$F0004E (vertical interrupt)
  0x91B4:'4e75',                  # RTS
  0x1BEB8:'33fc06c700f00028',      # following TOM write (not a VBlank wait)
}

def rom_code(path:Path):
 b=path.read_bytes();assert len(b)==4194304 and hashlib.sha256(b).hexdigest()==ROM_SHA
 z=zlib.decompressobj(31);code=z.decompress(b[0x3804:]);assert z.eof and len(code)==99072
 for at,h in {**R222,**R224,**NEW}.items():
  expected=bytes.fromhex(h)
  assert code[at-BASE:at-BASE+len(expected)]==expected,hex(at)
 return code

def original_branch(link:int, ypos:int, cc:int):
 """Exactly retail $927A instruction transformations; unlike R224 support CC=2."""
 assert link%8==0 and 0<=link<0x400000 and 0<=ypos<0x800 and 0<=cc<=7
 swap=((link&0xffff)<<16)|(link>>16)
 d2=((swap<<5)|(swap>>27))&0xffffffff
 first=d2&0xffff
 word=((cc<<16)>>2)|(d2&0xffff0000)|((ypos<<3)&0xffff)|3
 return first,word

def path_for_vc(vc:int,top:int,bottom:int)->tuple[str,...]:
 assert 0<=vc<2048
 if top>vc:return ('$7548','$7558 STOP')
 if bottom<=vc:return ('$7548','$7550','$7558 STOP')
 # The third branch at $7560 was built with YPOS=0,CC=2.
 # For active video, VC>0, so OP follows link $50C00 unconditionally.
 if vc>0:return ('$7548','$7550','$7560','$50C00 staged head')
 return ('$7548','$7550','$7560','fallthrough')

def audit(path:Path):
 code=rom_code(path)
 assert len(NEW)==16
 # verify the actual branch builder JSR target and that mode 2 was passed
 jsr=int.from_bytes(code[0x1BEB4-BASE:0x1BEB8-BASE],'big')
 assert jsr==0x927a
 assert 0x9192+2+int.from_bytes(code[0x9193-BASE:0x9194-BASE],'big',signed=True)==0x91a2
 assert original_branch(0x7558,28,1)==r224_branch(0x7558,28,1)
 assert original_branch(0x7560,508,1)==r224_branch(0x7560,508,1)
 derived=original_branch(0x50c00,0,2)
 assert fields(derived)==(0x50c00,0,2),(derived,fields(derived))
 assert derived[0] == 0x000000a1,(hex(derived[0]),hex(derived[1]))
 # Original source initializes *both* object-list heads with STOP (0,4),
 # enabling no-published-list to be a valid initial STOP. Later callback
 # paths can overwrite staging head, as separately proven by R241.
 assert int.from_bytes(bytes.fromhex(NEW[0x1BE78])[2:6],'big')==0
 assert int.from_bytes(bytes.fromhex(NEW[0x1BE82])[2:6],'big')==4
 assert int.from_bytes(bytes.fromhex(NEW[0x1BE8C])[2:6],'big')==0
 assert int.from_bytes(bytes.fromhex(NEW[0x1BE96])[2:6],'big')==4
 for label,top,bottom,height in (('NTSC',28,508,240),('PAL',44,604,280)):
  count=0
  for vc in range(0,2048,2):
   trace=path_for_vc(vc,top,bottom)
   if top<=vc<bottom:
    assert trace==('$7548','$7550','$7560','$50C00 staged head'),(label,vc,trace)
    count+=1
   else:
    assert trace[-1]=='$7558 STOP',(label,vc,trace)
  assert count==height
  saved_bottom_half=bottom//2 # original R224 $9106 LSR.W #1,D3 -> $2EE96
  vi=2*saved_bottom_half+3    # original R242 $91A2..$91AE
  assert vi==bottom+3 and vi%2==1 and vi<0x800
  print(f'PASS {label}: original VI=${vi:04X}, exactly 3 VC half-line units after VDE=${bottom:04X}, odd for noninterlaced')
  print(f'PASS {label}: {count}/{height} active VC scanlines route $7548 -> $7550 -> $7560 -> $50C00')
 print('PASS retail ROM signature,',len({**R222,**R224,**NEW}),'original CPU opcode groups including dynamic branch construction')
 print('PASS $1BE78..$1BE9E original source/staged list heads initialized to (0,4) STOP')
 print('PASS original $91A2..$91B4 computes VI=(saved VDE/2)*2+3 at TOM $F0004E')
 print('PASS $1BEA0..$1BEB6 constructs BRANCHOBJ at $7560: CC2, YPOS=0, LINK=$50C00')
 print('PASS third branch packed phrase words:', ' '.join(f'${v:08X}' for v in derived))
 print('PASS the OLP at $7548 is fixed; staged list at $50C00 is reached through active-video branch routing')
 print('LIMIT: synthetic logical paths, *not* observed runtime, stage/frame/display timing, TOM fetch arbitration or proof of no mid-scanline updates')

if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--rom',required=True,type=Path)
 audit(ap.parse_args().rom)
