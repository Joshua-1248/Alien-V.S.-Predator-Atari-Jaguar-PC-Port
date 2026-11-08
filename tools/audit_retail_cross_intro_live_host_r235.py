#!/usr/bin/env python3
"""R235 original intro call chain, direction/X geometry, actual compiled host renderer.

Uses personally provided SHA-verified retail ROM. No runtime instrumentation,
original copyrighted artwork, guessed scanlines or modifications to game code.
"""
from __future__ import annotations
import argparse
import hashlib
import pathlib
import re
import subprocess
import tempfile
import zlib
from audit_retail_mjp_transition_cpu import EXPECTED as CPU_POSITION, cpu_position
from audit_retail_cross_gpu_publication import EXPECTED_CPU as CPU_GPU, GPU as GPU_OPS
from audit_retail_mjp_gpu_handshake import ROM_SHA
from audit_retail_cross_gpu_dispatch import extract, disassemble, verify

BASE=0x8000
# All are complete original MOVE.L immediate-to-absolute or NEG.L opcodes.
WRITES={
    0x1D89A:(0x4BEDC,0), 0x1D8A4:(0x4BED4,0),
    0x1D8B4:(0x4BED4,-1),
    0x1D904:(0x4BED8,10), 0x1D930:(0x4BED8,99),
    0x1D9E4:(0x4BEDC,10),
    0x1DA0C:(0x4BEDC,10), 0x1DA16:(0x4BED8,10),
    0x1DA78:(0x4BED4,0),
    0x1DB40:(0x4BED4,0), 0x1DB82:(0x4BED4,0),
    0x1DBA6:(0x4BEDC,10), 0x1DBB0:(0x4BED8,10),
}
INTRO_CALLS=(0x1D90E,0x1D93A,0x1D95C,0x1D97E,0x1D9A0,0x1D9C2,0x1D9EE)
TEXT_IDS=(0x2AA89,0x2AB0F,0x2AB83,0x2AC14,0x2ACA6,0x2AD25,0x2ADA3)


def audit(rom_path:pathlib.Path,source:pathlib.Path,cc:str)->None:
    rom=rom_path.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA
    gz=zlib.decompressobj(31)
    code=gz.decompress(rom[0x3804:])
    assert gz.eof and len(code)==99072
    def raw(at,n):return code[at-BASE:at-BASE+n]
    for at,(addr,value) in WRITES.items():
        assert raw(at,10)==(b'\x23\xfc'+(value&0xffffffff).to_bytes(4,'big')+addr.to_bytes(4,'big')),(hex(at),hex(addr))
    assert raw(0x1E152,6)==bytes.fromhex('44b90004bed4')
    for at,hex_code in CPU_POSITION.items():assert raw(at,len(bytes.fromhex(hex_code)))==bytes.fromhex(hex_code),hex(at)
    for at,hex_code in CPU_GPU.items():assert raw(at,len(bytes.fromhex(hex_code)))==bytes.fromhex(hex_code),hex(at)
    # The actual page function invokes original transition, which runs the
    # 16-stage GPU and flips $4BED4 when GPU posts completed status 2.
    assert raw(0x1E206,6)==bytes.fromhex('4eb90001df2c')
    # Original intro initializes the picture buffers and first static
    # transition before changing direction to -1 at $1D8B4.
    assert raw(0x1D8AE,6)==bytes.fromhex('4eb90001df2c')
    for at,text_id in zip(INTRO_CALLS,TEXT_IDS):
        assert raw(at,6)==bytes.fromhex('4eb90001e1f2'),hex(at)
        # Page's string pointer is stored in $4BE70 before its call.
        prior=code[at-BASE-25:at-BASE]
        assert b'\x23\xfc'+text_id.to_bytes(4,'big')+bytes.fromhex('0004be70') in prior,(hex(at),hex(text_id))
    calls=[];needle=bytes.fromhex('4eb90001e1f2');offset=0
    while True:
        idx=code.find(needle,offset);offset=idx+6
        if idx<0:break
        address=BASE+idx
        if 0x1D890<=address<0x1DA00:calls.append(address)
    assert calls==list(INTRO_CALLS),[hex(x) for x in calls]
    gpu=extract(rom);ins,_=disassemble(gpu);verify(gpu,ins)
    for at,(op,s,d,imm) in GPU_OPS.items():
        assert (ins[at][0],ins[at][1],ins[at][2],ins[at][4])==(op,s,d,imm),hex(at)
    assert ins[0xF034C0][:3]==(0x02,1,20)
    assert ins[0xF034B0][:3]==(0x1e,21,20)
    assert ins[0xF034F4][:3]==(0x23,2,13)
    # Exact original intro call order: static initial transition mode 0,
    # assign mode -1, then 7 page calls. $1E152 NEG.L after each GPU done.
    direction=-1
    upper=10;lower=0
    names=((0,1),(1,2),(2,3),(3,4),(4,5),(5,6),(6,0))
    print('ORIGINAL_INITIAL_MODE0_X',','.join(map(str,[cpu_position(0,t,10,0) for t in range(8)])))
    print('ORIGINAL_INTRO_PAGE_CALLS',len(INTRO_CALLS))
    for ix,((frm,to),at) in enumerate(zip(names,INTRO_CALLS)):
        if ix==1: upper=99   # original $1D930 before second page
        if ix==6: lower=10  # original $1D9E4 before final page
        xs=[cpu_position(direction,t,upper,lower) for t in range(8)]
        print(f'INTRO_PAGE {frm}->{to} at ${at:06X} mode={direction:+d} upper={upper} lower={lower} OP_X={xs}')
        assert all(0<=x<320 for x in xs)
        # GPU-completion path negates direction at $1E152.
        direction=-direction
    assert direction==1
    # Verify actual game caller/event and original host static position.
    mjp=(source/'src/game/mjp.c').read_text()
    frontend=(source/'src/jaguar/avp_gpu/frontend_renderer.c').read_text()
    assert 'evt(0x4d4a5050u' in mjp and 'evt(0x4d4a5053u' in mjp
    # R280: CPU publishes X ($12+4*D1) and the $7FB4 stage per VBL (MJPX).
    assert 'evt(0x4d4a5058u,mjp_slide_x(d5),0);' in mjp
    assert 'evt(0x4d4a5058u,mjp_slide_x(stage+8),stage);' in mjp
    assert 'state->frontend_page_cross_phase+=2u;' not in frontend
    assert 'frontend_blit_scene_cross(fb,from,to,mode,state->frontend_picture_x,' in frontend
    # Execute the actual linked release source's renderer on synthetic frames,
    # not a reimplementation of its placement arithmetic.
    with tempfile.TemporaryDirectory(prefix='avp_r235_scene_probe_') as tmp:
        path=pathlib.Path(tmp)/'scene_probe'
        args=[cc,'-O2','-std=c99','-Wall','-Wextra','-Werror',f'-I{source/"include"}',
              str(source/'tools/retail_cross_live_host_probe_r235.c'),
              str(source/'src/jaguar/avp_gpu/frontend_renderer.c'),
              str(source/'src/jaguar/avp_gpu/frontend_fade.c'),
              str(source/'src/jaguar/hw/cry.c'),
              str(source/'src/jaguar/hw/framebuffer.c'),
              str(source/'tools/retail_cross_rgb16_reference_r234.c'),'-o',str(path),'-lm']
        subprocess.run(args,check=True,capture_output=True,text=True)
        result=subprocess.run([str(path)],check=True,capture_output=True,text=True)
        rows=re.findall(r'TICK=(\d+) PHASE=(\d+) HOST_X=(\d+)\.\.(\d+) HOST_RGB16=([0-9A-F]+) ORIGINAL_GPU_RGB16=([0-9A-F]+) MATCH=(\d+)',result.stdout)
        assert len(rows)==24,result.stdout
        assert [(int(t),int(p)) for t,p,*_ in rows]==[(i+1,0 if i<8 else i-7) for i in range(24)]
        xs=[int(x0) for _,_,x0,*_ in rows]
        assert xs==[58]*8+[18+4*min(16-p,10) for p in range(1,17)]
        assert all(int(x1)==int(x0)+191 for _,_,x0,x1,*_ in rows)
        assert all(int(v[-1])==1 for v in rows)
        print('ACTUAL_COMPILED_PC_RENDERER_24_VBLS')
        print(result.stdout,end='')
    print('PASS canonical ROM, 13 original mode/limit writes, 7 intro page calls, 22 CPU geometry sites, 16-stage GPU code')
    print('PASS R280 production renderer: retail page-1 OP X per VBL and packed GPU colour at every stage')
    print('LIMIT: this is CPU OP geometry + packed arithmetic; not a coupled physical scanout/Blitter timing or captured Jaguar frame')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=pathlib.Path,required=True)
    p.add_argument('--source',type=pathlib.Path,required=True)
    p.add_argument('--cc',default='gcc')
    a=p.parse_args();audit(a.rom,a.source,a.cc)
