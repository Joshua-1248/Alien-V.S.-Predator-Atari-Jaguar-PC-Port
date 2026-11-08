#!/usr/bin/env python3
"""RE24 S27: ROM-free proof generated code changes register banks after FLAGS store.

Both the switching and non-switching cases are executed, not text-matched.
The tests use fabricated 16-bit JRISC instructions, not extracted ROM data.
"""
import os
import pathlib
import subprocess
import sys
import tempfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'jagref'))
import gpu_recomp

BASE = 0xF03000

def enc(op, r1, r2):
    return ((op << 10) | (r1 << 5) | r2).to_bytes(2, 'big')

def movei(val, dest):
    return enc(38, 0, dest) + (val & 65535).to_bytes(2, 'big') + ((val >> 16) & 65535).to_bytes(2, 'big')

def run_case(register_page):
    # MOVEI $F02100,r1 ; MOVEI FLAG,r2 ; STORE r2,(r1);
    # MOVEQ 7,r3 ; stop at the end of the generated segment.
    program = movei(0xF02100, 1) + movei(register_page << 14, 2) + enc(47, 1, 2) + enc(35, 7, 3)
    image = program + bytes(4096-len(program))
    code = gpu_recomp.emit(image, BASE, [(BASE, BASE+len(program))], 'synthetic', origin='synthetic, no retail data')
    test_source = r'''#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef struct {
 uint32_t r[2][32];unsigned bank;int z,c,n,divoff;
 uint32_t remain;int64_t acc;uint32_t fault;
 volatile int stop;uint32_t param[256];
 void *ctx;uint32_t (*rd)(void*,uint32_t,int);void (*wr)(void*,uint32_t,uint32_t,int);
} AvpGpuRecompCtx;
#define BANK (g->bank)
static void wr(void *ctx,uint32_t a,uint32_t value,int size){
 AvpGpuRecompCtx *g=(AvpGpuRecompCtx*)ctx;
 assert(a==0xF02100u && size==4);
 g->bank=(value>>14)&1u;
}
''' + code + f'''
int main(void){{
 AvpGpuRecompCtx g;memset(&g,0,sizeof g);
 g.ctx=&g;g.wr=wr;
 assert(synthetic(&g,0xF03000u)==0x{BASE+len(program):06X}u);
 assert(g.bank=={register_page}u);
 assert(g.r[{register_page}][3]==7u);
 assert(g.r[{register_page ^ 1}][3]==0u);
 return 0;
}}
'''
    with tempfile.TemporaryDirectory(prefix='avp_s27_synthetic_') as td:
        src=pathlib.Path(td)/'test.c';out=pathlib.Path(td)/'run'
        src.write_text(test_source)
        cc=os.environ.get('CC','cc')
        subprocess.run([cc,'-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-label','-Wno-implicit-fallthrough',str(src),'-o',str(out)],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        subprocess.run([str(out)],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

if __name__=='__main__':
    run_case(0)
    run_case(1)
    print('RE24 S27 generated GPU FLAGS bank refresh: PASS (both pages, ROM-free)')
