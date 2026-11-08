#!/usr/bin/env python3
"""R234: retail CROSS GPU RGB16 arithmetic vs the *compiled* host C helper.

The user supplies their authentic retail ROM; no art or GPU code leaves this
read-only audit. Compile the unmodified PC fade source to a temporary shared
library, run all 16 original runtime GPU kernels on synthetic packed pixel
pairs, and compare with the exact compiled C helper used by the PC frontend.
This determines arithmetic parity ONLY; it does not prove which frontend host
call maps to CROSS, or GPU/Blitter/TOM scanout and timing. No runtime patches.
"""
from __future__ import annotations
import argparse
import ctypes
import hashlib
from pathlib import Path
import random
import subprocess
import tempfile
import zlib
from audit_retail_cross_gpu_dispatch import extract, disassemble, verify, gpu_blend_word
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_cross_gpu_publication import GPU as GPU_SITES


def make_c_helper(root: Path, directory: Path, cc: str):
    src = root/'src/jaguar/avp_gpu/frontend_fade.c'
    header = root/'include/jaguar/avp_gpu/frontend_fade.h'
    renderer = (root/'src/jaguar/avp_gpu/frontend_renderer.c').read_text()
    text=src.read_text()
    assert 'u16 avp_gpu_rgb16_cross(u16 from,u16 to,unsigned phase)' in text
    assert 'avp_gpu_rgb16_fraction(from,phase)' in text
    assert 'avp_gpu_rgb16_fraction(to,phase)' in text
    assert 'avp_gpu_rgb16_cross(a,b,phase)' in renderer
    # R280: the $01DF2C scene CROSS uses the retail stage kernels.
    assert 'u16 avp_gpu_rgb16_scene_cross(u16 old_px,u16 new_px,unsigned stage)' in text
    assert 'avp_gpu_rgb16_scene_cross(a,b,phase)' in renderer
    assert header.is_file()
    library=directory/'host_fade_exact.so'
    subprocess.run([cc,'-shared','-fPIC','-O2','-std=c11','-Wall','-Wextra',
                    '-Werror','-I'+str(root/'include'),str(src),str(root/'tools/retail_cross_rgb16_reference_r234.c'),'-o',str(library)],check=True,
                   stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    lib=ctypes.CDLL(str(library))
    fn=lib.avp_gpu_rgb16_cross
    fn.restype=ctypes.c_uint16
    fn.argtypes=(ctypes.c_uint16,ctypes.c_uint16,ctypes.c_uint)
    original=lib.r234_original_cross_word
    original.restype=ctypes.c_uint32
    original.argtypes=(ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint)
    scene=lib.avp_gpu_rgb16_scene_cross
    scene.restype=ctypes.c_uint16
    scene.argtypes=(ctypes.c_uint16,ctypes.c_uint16,ctypes.c_uint)
    return fn,original,scene,hashlib.sha256(src.read_bytes()).hexdigest()


def pair(fn, old: int, new: int, phase: int)->int:
    return (fn(old>>16,new>>16,phase)<<16)|fn(old&0xffff,new&0xffff,phase)


def cases(w: int, h: int):
    """Synthetic test art with position-dependent, 16-bit packed pixel pairs."""
    for y in range(h):
        for x in range(0,w,2):
            p=((x*2137+y*3191+(x^y)*779)&0xffff)
            q=(((x+1)*6329+y*2917+((x+1)^y)*137)&0xffff)
            r=((x*4523+y*929+(x+71)*179)&0xffff)
            s=(((x+1)*787+y*3529+((x+1)^y)*1531)&0xffff)
            yield (p<<16)|q,(r<<16)|s


def audit(rom:Path,source:Path,cc:str):
    data=rom.read_bytes()
    assert len(data)==4194304 and hashlib.sha256(data).hexdigest()==ROM_SHA
    gpu=extract(data)
    ins,_=disassemble(gpu)
    verify(gpu,ins)
    assert ins[0xF030BE][:3]==(0x26,0,21) and ins[0xF030BE][4]==16
    assert ins[0xF030C4][:3]==(0x23,1,20)
    assert ins[0xF030E4][:3]==(0x2f,12,13)
    assert ins[0xF03476][:3]==(0x1f,1,20)
    assert len(GPU_SITES)>=30
    with tempfile.TemporaryDirectory(prefix='avp_r234_host_color_') as temp:
        fn,reference,scene,source_hash=make_c_helper(source,Path(temp),cc)
        rng=random.Random(0xF03000)
        mismatch=[]
        representative=[]
        for phase in range(17):
            misses=0
            example=None
            for _ in range(1024):
                old,new=rng.getrandbits(32),rng.getrandbits(32)
                genuine=gpu_blend_word(gpu,ins,phase,old,new)
                assert reference(old,new,phase)==genuine,(phase,old,new)
                assert pair(scene,old,new,phase)==genuine,('scene',phase,old,new)
                hosted=pair(fn,old,new,phase)
                if genuine!=hosted:
                    misses+=1
                    if example is None:example=(old,new,genuine,hosted)
            mismatch.append(misses)
            if example is not None:representative.append((phase,*example))
        assert mismatch[0]==mismatch[16]==0
        # These are measurements against the shipped C and the authentic
        # unchanged GPU code, not target predictions or guessed coefficients.
        assert sum(mismatch)>0,'Unexpected perfect PC-vs-GPU color parity'
        print('HOST_SOURCE_SHA256',source_hash)
        print('PHASE_MISMATCHES_1024_RANDOM_PAIRS',','.join(map(str,mismatch)))
        print('FIRST_DIFFERENCES_PER_PHASE')
        for ph,old,new,jag,host in representative:
            print(f'  phase={ph:2d} from={old:08X} to={new:08X} JAG={jag:08X} HOST={host:08X}')
        for w,h in ((192,144),(320,144)):
            totals=[0]*17
            crc_jag=[0]*17
            crc_host=[0]*17
            # All 144 rows for every runtime phase. Full original RGB16 GPU
            # kernel arithmetic per 32-bit pair; synthetic scene, not Jaguar art.
            for old,new in cases(w,h):
                for phase in range(1,17):
                    a=gpu_blend_word(gpu,ins,phase,old,new)
                    assert reference(old,new,phase)==a,(w,h,phase,old,new)
                    assert pair(scene,old,new,phase)==a,('scene',w,h,phase)
                    b=pair(fn,old,new,phase)
                    totals[phase]+=(a!=b)
                    crc_jag[phase]=zlib.crc32(a.to_bytes(4,'big'),crc_jag[phase])
                    crc_host[phase]=zlib.crc32(b.to_bytes(4,'big'),crc_host[phase])
            pairs=w*h//2
            print(f'FULL_SYNTHETIC_FRAME {w}x{h}, {pairs} pairs, 16 runtime phases, '
                  f'{16*pairs} word comparisons')
            for phase in range(1,17):
                print(f'  stage={phase:2d} mismatch={totals[phase]:5d}/{pairs} '
                      f'JAG_crc32={crc_jag[phase]:08x} PC_crc32={crc_host[phase]:08x}')
            assert crc_jag[16]==crc_host[16]
            assert all(totals[i]==0 for i in (1,2,3,4,5,6,8,9,16))
            assert all(totals[i]>0 for i in (7,10,11,12,13,14,15))
        print('PASS independent C packed-word reference matches actual retail GPU kernel 17/17 phases')
        print('PASS authentic retail GPU RISC kernels vs compiled, unchanged host C crossfade')
        print('PASS whole synthetic 192x144 and 320x144 frames for all 16 runtime stages')
        print('PASS R280 production avp_gpu_rgb16_scene_cross == retail GPU kernel, 17/17 stages, random words and full frames')
        print('NOTE: legacy proportional avp_gpu_rgb16_cross still differs in 7 stages; no longer used by the $01DF2C scene CROSS')
        print('BOUNDARY: host caller phase mapping, original pixel inputs, Blitter/OP timing not established')

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',required=True,type=Path)
    ap.add_argument('--source',type=Path,required=True)
    ap.add_argument('--cc',default='cc')
    args=ap.parse_args()
    audit(args.rom,args.source,args.cc)
