#!/usr/bin/env python3
"""No retail instructions: generated GPU program reads staged maze, texture,
object and camera bytes from a synthetic, caller-declared Jaguar layout."""
import os, struct, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'jagref'))
import gpu_recomp
BASE=0xF03000
START=0xF03220
image=bytearray(0x1000)
def ins(addr,op,r1=0,r2=0):
    struct.pack_into('>H',image,addr-BASE,(op<<10)|(r1<<5)|r2)
def movei(addr,dst,val):
    ins(addr,38,0,dst)
    struct.pack_into('>H',image,addr-BASE+2,val&0xFFFF)
    struct.pack_into('>H',image,addr-BASE+4,val>>16)
movei(BASE,20,START);ins(BASE+6,52,20,0);ins(BASE+8,57)
addr=START
for i,src in enumerate((0x1000,0x4000,0x6000,0x7FB0)):
    movei(addr,14,src);addr+=6
    movei(addr,15,0x500+i*4);addr+=6
    ins(addr,41,14,1);addr+=2
    ins(addr,47,15,1);addr+=2
movei(addr,20,0xF03118);addr+=6
ins(addr,52,20,0);addr+=2
ins(addr,57);addr+=2
while (addr-START)%4:
    ins(addr,57);addr+=2
assert (addr-START)%4==0
with tempfile.TemporaryDirectory(prefix='re24s21-layout-') as tmp:
    tmp=Path(tmp)
    gen=tmp/'generated.c'
    gen.write_text(gpu_recomp.emit(image,BASE,[(BASE,BASE+10),(START,addr)],
        'avp_gpu_s21_stage',(0xF03118,),
        'S21 ROM-free original scene-layout staging regression'))
    (tmp/'overlay.bin').write_bytes(image[START-BASE:addr-BASE])
    (tmp/'resident.bin').write_bytes(image[:0x220])
    compiler=os.environ.get('CC','gcc')
    exe=tmp/'checker'
    cmd=[compiler,'-std=c99','-O2','-Wall','-Wextra','-Werror',
         '-Wno-unused-label','-Wno-implicit-fallthrough',
         '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
         '-I'+str(ROOT/'include'),'-DGEN_C="'+str(gen)+'"',
         '-DOVERLAY_BYTES='+str(addr-START),
         str(ROOT/'tests'/'test_gpu_original_scene_layout_generated_s21.c'),
         str(ROOT/'src/jaguar/avp_gpu/original_view_scene.c'),
         str(ROOT/'src/jaguar/avp_gpu/original_view_native.c'),
         str(ROOT/'src/jaguar/avp_gpu/original_view_layout.c'),
         str(ROOT/'src/game/collision.c'),'-o',str(exe)]
    subprocess.run(cmd,check=True)
    subprocess.run([str(exe),str(tmp/'overlay.bin'),str(tmp/'resident.bin')],check=True)
    print('RE24 S21 generated GPU reads mapped scene PASS ('+compiler+')')
