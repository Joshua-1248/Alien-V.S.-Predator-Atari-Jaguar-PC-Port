#!/usr/bin/env python3
"""R241 ROM-backed CROSS callback guard and ACK non-equivalence sweep.

Verifies actual 68000 instruction windows, branch targets and copy exits, then
runs guarded callback truth-table cases on SYNTHETIC list words. This is NOT a
Jaguar cycle emulator or claim of observed hardware scanout or interrupt rates.
No original ROM bytes are copied into the output tree.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import zlib
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_cross_op_list_publication import ORIGINAL as R222_ORIGINAL

BASE=0x8000
ACK=0x7FA8
# Addresses align to full 68000 opcodes, including extension words.
CHECKS={
    0x1C130:'23fc0001e34a0002ee80',  # $7FAC mode 2 -> six-record handler
    0x1C25A:'23fc0001e34a0002ee80',  # $7FAC mode 4 -> six-record handler
    0x1E016:'207c0004be20',          # load the original patch-enable flag address
    0x1E01C:'20bc00000001',          # CROSS writes flag = 1 before GPU launch
    0x1E02E:'23fc00f0300000f02110',  # GPU startup PC, later than flag = 1
    0x1E34A:'247c0004fc00',          # six-record source head
    0x1E350:'2012',                  # MOVE.L (A2),D0
    0x1E352:'6700006a',              # if empty, skip patch AND copy
    0x1E356:'207c0004be20',          # patch guard address
    0x1E35C:'0c9000000000',          # CMPI.L #0,(A0)
    0x1E362:'67000024',              # if flag=0, skip patch, DO copy
    0x1E36C:'d5fc00000040',          # index 2, third record, +64 bytes
    0x1E372:'2012c0bc000007ff',      # preserve low 11 bits of picture record
    0x1E37A:'227c00007fa4',          # GPU-published pointer address
    0x1E380:'2211e18980812480',      # read pointer, shift 8, OR, source write
    0x1E388:'247c0004fc00267c00050c007606', # six records source/dest
    0x1E396:'204a224b2018d0bc0000000222c022d822d822d8', # copy
    0x1E3BE:'21fc000000017fa84e75', # unconditional shared exit: ACK+RTS
    0x1E4D0:'247c0004fc00267c00050c00', # 8-record writer no head guard
    0x1E4E2:'7608', # record count 8
    0x1E520:'21fc000000017fa84e75', # eight-record ACK+RTS
    0x1E804:'247c0004fc00',          # ten-record source head
    0x1E80A:'2012',                  # MOVE.L (A2),D0
    0x1E80C:'67000050',              # if empty, skip patch AND copy
    0x1E816:'227c00007fa4',          # ten-record unconditional patch when nonempty
    0x1E81C:'2211e18980812480',      # pointer combine
    0x1E824:'247c0004fc00267c00050c00760a', # ten records
    0x1E85E:'21fc000000017fa84e75', # ten-record ACK+RTS
}


def load_rom(path:Path)->bytes:
    rom=path.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA, 'wrong retail ROM'
    gz=zlib.decompressobj(31)
    code=gz.decompress(rom[0x3804:])
    assert gz.eof and len(code)==99072
    for at,h in {**R222_ORIGINAL,**CHECKS}.items():
        b=bytes.fromhex(h)
        assert code[at-BASE:at-BASE+len(b)]==b,(hex(at),'unexpected retail instructions')
    return code


def branch_target(code:bytes,at:int)->int:
    opcode,disp=struct.unpack_from('>Hh',code,at-BASE)
    assert opcode==0x6700,(hex(at),hex(opcode))
    return at+2+disp  # Motorola 68000: relative to extension-word address


def run_case(kind:str,head:int,flag:int)->tuple[bool,bool,bool,bool]:
    # 32-byte spacing and first-word +2 from original callback copy bodies.
    src=[[(0x10000000+i*0x10000+j*23)&0xffffffff for j in range(8)] for i in range(11)]
    src[0][0]=head
    dst=[[0xBADC0DED]*8 for _ in range(11)]
    before_src=[row[:] for row in src]
    picture=0x00031400  # synthetic distinct buffer, not original scene memory
    patched=False;copied=False;ack=False
    if kind=='six':
        if src[0][0]!=0:
            if flag!=0:
                src[2][0]=(src[2][0]&0x7ff)|((picture<<8)&0xffffffff)
                patched=True
            count,words=6,4
            for i in range(count):
                dst[i][0]=(src[i][0]+2)&0xffffffff
                dst[i][1:words]=src[i][1:words]
            copied=True
        ack=True  # all guard branches join $1E3BE
    elif kind=='ten':
        if src[0][0]!=0:
            src[0][0]=(src[0][0]&0x7ff)|((picture<<8)&0xffffffff)
            patched=True
            for i in range(10):
                dst[i][0]=(src[i][0]+2)&0xffffffff
                dst[i][1:6]=src[i][1:6]
            copied=True
        ack=True  # guarded branch joins $1E85E
    elif kind=='eight':
        for i in range(8):
            dst[i][0]=(src[i][0]+2)&0xffffffff
            dst[i][1:6]=src[i][1:6]
        dst[8][:6]=src[8][:6] # original final tail
        copied=ack=True
    else:raise ValueError(kind)
    assert ack
    assert (dst!=[[0xBADC0DED]*8 for _ in range(11)])==copied
    if not copied:
        assert src==before_src
    if kind=='six': assert patched==(head!=0 and flag!=0)
    if kind=='ten': assert patched==(head!=0)
    if kind=='eight': assert not patched
    return copied,patched,ack,not copied and ack


def main(path:Path):
    code=load_rom(path)
    assert branch_target(code,0x1E352)==0x1E3BE
    assert branch_target(code,0x1E362)==0x1E388
    assert branch_target(code,0x1E80C)==0x1E85E
    # No need to assume any cycle timing: branch outcomes from original opcodes.
    results=[]
    for kind in ('six','eight','ten'):
        for head in (0,0x90000000):
            for flag in (0,1):
                copied,patched,ack,ack_without_copy=run_case(kind,head,flag)
                results.append((kind,head,flag,copied,patched,ack,ack_without_copy))
    assert len(results)==12
    assert sum(x[-1] for x in results)==4,(results) # six and ten, empty x both flags
    assert sum(x[4] for x in results)==3 # six 1 and ten 2
    assert all(x[5] for x in results)
    assert all(not x[3] for x in results if x[0] in ('six','ten') and x[1]==0)
    assert all(x[3] for x in results if x[0]=='eight')
    assert all(x[3] for x in results if x[1]!=0)
    print('PASS verified original retail ROM and',len({**R222_ORIGINAL,**CHECKS}),'instruction groups')
    print('PASS six-record head-empty BEQ -> $1E3BE ACK+RTS, *skips* list copy')
    print('PASS six-record patch-flag-zero BEQ -> $1E388 (copy without $7FA4 pointer patch)')
    print('PASS ten-record head-empty BEQ -> $1E85E ACK+RTS, *skips* list copy')
    print('PASS eight-record copies list, then ACK (no head-empty BEQ)')
    print('PASS original CROSS writes $4BE20=1 before starting GPU, enabling conditional six-record pointer patch while flag remains set')
    print('PASS 12 synthetic guard outcomes; four ACK-without-copy cases, all ACK writes unconditional at shared exits')
    print('FINDING: $7FA8=1 alone does NOT prove a picture was copied to staged TOM list')
    print('BOUNDARY: no claim about actual empty lists, callback lifetime, interrupt arrivals, or TOM scanline visibility')


if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--rom',required=True,type=Path)
    main(a.parse_args().rom)
