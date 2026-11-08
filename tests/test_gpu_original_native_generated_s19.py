#!/usr/bin/env python3
"""Check the real JRISC->C generated function through the native view driver.
No commercial GPU instructions or ROM content are used or distributed."""
import os,struct,sys,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'jagref'))
import gpu_recomp
B=0xF03000
buf=bytearray(0x1000)
def ins(off,op,r1=0,r2=0):struct.pack_into('>H',buf,off-B,(op<<10)|(r1<<5)|r2)
def movei(off,dst,v):
    ins(off,38,0,dst)
    struct.pack_into('>H',buf,off-B+2,v&0xffff)
    struct.pack_into('>H',buf,off-B+4,v>>16)
movei(B,20,0xF03220)
ins(B+6,52,20,0)
ins(B+8,57)
movei(0xF03220,1,0x12345678)
movei(0xF03226,14,0x400)
ins(0xF0322C,47,14,1)
movei(0xF0322E,20,0xF03118)
ins(0xF03234,52,20,0)
ins(0xF03236,57)
source=gpu_recomp.emit(buf,B,[(B,B+10),(0xF03220,0xF03238)],
                       'avp_gpu_stage0',(0xF03118,),
                       'from original-free ROM regression',
                       movei_params={0xF03220:0})
with tempfile.TemporaryDirectory(prefix='s19-generated-') as d:
    file=Path(d)/'generated.c';file.write_text(source+'\n')
    cc=os.environ.get('CC','gcc')
    cmd=[cc,'-std=c99','-O2','-Wall','-Wextra','-Werror',
         '-Wno-unused-label','-Wno-implicit-fallthrough','-I'+str(ROOT/'include'),
         '-DGEN_C="'+str(file)+'"',str(ROOT/'tests'/'test_gpu_original_native_s19.c'),
         str(ROOT/'src/jaguar/avp_gpu/original_view_native.c'),'-o',str(Path(d)/'test')]
    subprocess.run(cmd,check=True)
    subprocess.run([str(Path(d)/'test')],check=True)
    print(f'RE24 S19 real generated JRISC-to-C stage integration PASS ({cc})')
