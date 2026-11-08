#!/usr/bin/env python3
"""Offline, source-backed Atari Jaguar AVP retail CROSS GPU dispatch audit.

Reads only the user's original AVP.jag. Disassembles the GPU RISC instruction
boundaries and evaluates its 17 original *single 32-bit word* blend kernels.
This is not a cycle-accurate GPU/Blitter/OP emulator or a host game patch.
Original code, art, ROM bytes and instruction dumps are never distributed.
"""
from __future__ import annotations
import argparse
from collections import Counter
from hashlib import sha256
from pathlib import Path
from struct import unpack_from
import zlib

ROM_HASH='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
IMAGE_HASH='0300cb1bf633acbd620f8185b144737fabb596ffce7cb52708d2bf44afb321e5'
GPU_BASE=0xF03000
TABLE_START=0xF03538
TABLE_END=0xF0357C
# These are *decoded GPU image addresses*, not source offsets or output data.
EXPECTED_KERNELS=(0xF03170,0xF0318A,0xF031B0,0xF031D6,
                  0xF03208,0xF0322E,0xF03260,0xF03292,
                  0xF032C4,0xF032EA,0xF0331C,0xF0334E,
                  0xF03380,0xF033A6,0xF033D8,0xF033FE,
                  0xF03424)
MASKS={28:0x7BDF7BDF,27:0x39CF39CF,26:0x18C718C7,25:0x08430843}
# Real Jaguar instruction opcodes; aligned 16-bit op, 2-word MOVEI payload.
MOVEI=0x26
LOAD=0x29
STORE=0x2F
MOVE=0x22
SHRQ=0x19
AND=0x09
SUB=0x04
ADD=0x00
JR=0x35
JUMP=0x34
NOP=0x39


def extract(rom: bytes) -> bytes:
    if len(rom)!=4*1024*1024 or sha256(rom).hexdigest()!=ROM_HASH:
        raise ValueError('Original verified 4 MiB retail AVP.jag required')
    decoder=zlib.decompressobj(31)
    rdb=decoder.decompress(rom[0xE840:])
    if not decoder.eof or len(rdb)!=60248:
        raise ValueError('Invalid original retail RDB.DTA stream')
    offset=0x2B828-0x20300
    dest,length=unpack_from('>II',rdb,offset)
    if (dest,length)!=(GPU_BASE,1488):
        raise ValueError('Original CROSS GPU header changed')
    payload=rdb[offset+8:offset+8+length]
    if sha256(payload).hexdigest()!=IMAGE_HASH:
        raise ValueError('Original CROSS GPU program changed')
    return payload


def rd16(image: bytes,addr: int) -> int:
    offset=addr-GPU_BASE
    if offset<0 or offset+2>len(image) or offset&1:
        raise ValueError(f'GPU address outside verified image: ${addr:06X}')
    return unpack_from('>H',image,offset)[0]


def rd32(image: bytes,addr: int) -> int:
    return (rd16(image,addr)<<16)|rd16(image,addr+2)


def disassemble(image: bytes) -> tuple[dict[int,tuple[int,int,int,int,int|None]],Counter]:
    p=GPU_BASE
    out={};hist=Counter()
    while p<GPU_BASE+len(image):
        ins=rd16(image,p)
        op=ins>>10
        arg=(ins>>5)&31
        dst=ins&31
        extra=None
        size=2
        if op==MOVEI:
            # Original Jaguar MOVEI payload is low 16 bits *before* high 16.
            extra=rd16(image,p+2)|(rd16(image,p+4)<<16)
            size=6
        out[p]=(op,arg,dst,size,extra)
        hist[op]+=1
        p+=size
    if p!=GPU_BASE+len(image):
        raise ValueError('Unaligned GPU decode')
    return out,hist


def verify(image: bytes,decoded: dict[int,tuple[int,int,int,int,int|None]]) -> None:
    table=tuple(rd32(image,at) for at in range(TABLE_START,TABLE_END,4))
    if table!=EXPECTED_KERNELS:
        raise ValueError('17 original CROSS dispatch addresses changed')
    if rd32(image,TABLE_END-4)!=0xF03424:
        raise ValueError('Last full-copy entry missing')
    for ptr in table:
        if ptr not in decoded or decoded[ptr][0] not in (LOAD,):
            raise ValueError(f'GPU dispatch target is not a load at ${ptr:X}')
    # Decode checked instruction sites corresponding to actual table selection,
    # per-word work, synchronization, and final CPU status 2 / GPU halt.
    instrs={
        0xF03000:(MOVEI,0,13,0xF03538),
        0xF0300E:(MOVEI,0,12,0x7F80),
        0xF03014:(0x23,1,13,None), # MOVEQ #1 status on entry
        0xF030E0:None, # see exact store at $F030E4
        0xF030E4:(STORE,12,13,None), # progress $7FB4
        0xF0316C:(JUMP,0,0,None), # phase pointer dispatch, r0
        0xF034F4:(0x23,2,13,None), # GPU completion status=2
        0xF034F6:(STORE,12,13,None),
        0xF03510:(MOVEI,0,31,0xF02114),
        0xF03516:(STORE,31,30,None), # halt GPU
    }
    del instrs[0xF030E0]
    for at,expected in instrs.items():
        o,a,b,_,e=decoded[at]
        if (o,a,b,e)!=expected:
            raise ValueError(f'GPU opcode/operand mismatch at ${at:06X}: {(o,a,b,e)}')
    for addr in (0xF03072,0xF03078,0xF0307E,0xF03084):
        # addresses point to MOVEI payloads, not four standalone opcodes
        if addr-2 not in decoded or decoded[addr-2][0]!=MOVEI:
            raise ValueError('RGB16 fraction constant is not a GPU MOVEI operand')
    for reg,at in zip((28,27,26,25),(0xF03070,0xF03076,0xF0307C,0xF03082)):
        if decoded[at][4]!=MASKS[reg] or decoded[at][2]!=reg:
            raise ValueError(f'Original GPU RGB16 mask corrupted at ${at:X}')
    for at,(op,a,cc,size,_) in decoded.items():
        if at>=0xF03538 and at<TABLE_END:
            continue # 17 long jump pointers intentionally share instruction space
        if op==JR:
            signed=a if a<16 else a-32
            target=at+2+signed*2
            if target not in decoded:
                raise ValueError(f'GPU JR target not on instruction boundary at ${at:X}')


def gpu_blend_word(image: bytes,decoded: dict,phase: int,old: int,new: int) -> int:
    """Execute the *GPU blend kernel only*, with source loads from two input words.

    Does not claim frame timing, OP publishing, async Blitter register state, or
    CPU->GPU shared-memory scheduling; phase comes from original 17-way table.
    """
    if not 0<=phase<=16:raise ValueError('phase outside original dispatch range')
    registers=[0]*32
    for r,mask in MASKS.items():registers[r]=mask
    # Parent loop arranges these as per-word old/new addresses, before dispatch.
    registers[12]=0x00100000
    registers[19]=0x00200000
    pc=EXPECTED_KERNELS[phase]
    for _ in range(60):
        op,s,d,_,extra=decoded[pc]
        if op==LOAD:
            if s not in (12,19) or d not in (7,13,22):
                raise ValueError(f'unhandled blend-kernel LOAD: ${pc:X}')
            registers[d]=(old if s==12 else new)
        elif op==MOVE:
            registers[d]=registers[s]
        elif op==SHRQ:
            registers[d]=(registers[d]>>(s or 32))&0xffffffff
        elif op==AND:
            registers[d]&=registers[s]
        elif op==SUB:
            registers[d]=(registers[d]-registers[s])&0xffffffff
        elif op==ADD:
            registers[d]=(registers[d]+registers[s])&0xffffffff
        elif op==STORE:
            if s!=12 or d!=22:
                raise ValueError(f'unexpected blend-kernel STORE at ${pc:X}')
            return registers[22]
        else:
            raise ValueError(f'unhandled GPU blend-kernel opcode {op} at ${pc:X}')
        pc+=2
    raise ValueError('CROSS kernel did not write one destination word')


def check_blend(image: bytes,decoded: dict) -> None:
    # Fixed pair of 16-bit RGB pixels (32-bit GPU memory word).
    old=0x1234ABCD
    new=0x4321FEDC
    samples=[(old,new),(0,0xffffffff),(0xffffffff,0),(0x00010001,0xFFFF0000)]
    for stage in range(17):
        for a,b in samples:
            gpu_blend_word(image,decoded,stage,a,b)
    if gpu_blend_word(image,decoded,0,old,new)!=old:
        raise ValueError('CROSS dispatch phase zero is not original old-image copy')
    if gpu_blend_word(image,decoded,16,old,new)!=new:
        raise ValueError('CROSS dispatch phase sixteen is not new-image copy')
    # Every word below is derived by executing the original 68000-loaded GPU
    # kernel on two 32-bit RGB16 source words, *not* from the host renderer.
    expected=(0x1234ABCD,0x1233AC0E,0x1A32B44F,0x1A31B490,
              0x226FBC91,0x226EBCD2,0x2A6DC513,0x2AACD513,
              0x2AAAD555,0x2AA9D596,0x2AE8E596,0x32E7EDD7,
              0x32E6EE18,0x3B24F619,0x3B23F65A,0x4322FE9B,0x4321FEDC)
    observed=tuple(gpu_blend_word(image,decoded,phase,old,new)
                   for phase in range(17))
    if observed!=expected:
        raise ValueError('Retail GPU single-word transition outputs differ')
    # Deliberately do not assert host crossfade equivalence; it is unverified.
    print(f'PASS 17 stage outputs for original RGB16 word pair, including stage 7 ${observed[7]:08X}')


def run(rompath: Path) -> None:
    image=extract(rompath.read_bytes())
    decoded,hist=disassemble(image)
    verify(image,decoded)
    check_blend(image,decoded)
    print(f'PASS original CROSS GPU image: {len(image)} bytes, SHA-256 {IMAGE_HASH}')
    excluded=sum(0xF03538<=at<0xF0357C or 0xF03534<=at<0xF03538 or at>=0xF035CC for at in decoded)
    print(f'PASS {len(decoded)-excluded} executable GPU instructions; {excluded} table/padding words excluded (MOVEI extensions included)')
    print(f'PASS 17 dispatch entries at $F03538..$F0357B; first ${EXPECTED_KERNELS[0]:06X}, last ${EXPECTED_KERNELS[-1]:06X}')
    print('PASS original GPU progress at $7FB4, completion value 2 at $7F80, hardware GPU stop')
    print('BOUNDARY: no full GPU/Blitter/OP timing or live-video equivalence asserted')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',required=True,type=Path)
    run(parser.parse_args().rom)
