#!/usr/bin/env python3
"""R236: ROM-authenticated two-epoch CPU/GPU CROSS lifetime and host handoff.

An offline 68000/GPU source-order / synthetic state verifier, NOT a cycle-accurate
Jaguar emulator or original displayed-frame claim. Uses the user's private ROM.
No original executable, images or audio are written to the output tree.
"""
from __future__ import annotations
import argparse, hashlib, pathlib, re, zlib
from audit_retail_mjp_transition_cpu import EXPECTED as CPU_POS, cpu_position
from audit_retail_cross_gpu_publication import EXPECTED_CPU as CPU_GPU, GPU as GPU_INS
from audit_retail_cross_gpu_dispatch import extract, disassemble, verify, EXPECTED_KERNELS
from audit_retail_cross_intro_live_host_r235 import WRITES, INTRO_CALLS
from audit_retail_mjp_gpu_handshake import CPU as CPU_COMMAND, ROM_SHA

BASE=0x8000
# Complete original opcodes: original CPU 8 ticks; then GPU command/start;
# then CPU publishes the GPU-owned bitmap pointer and polls completion.
CHECKS={
 0x01DF2C:'7a00',
 0x01DFCE:'dabc00000001',
 0x01DFD4:'4eb90001e15a',
 0x01DFDA:'babc000000086600ff4c',
 0x01DFE4:'207c00007f80',
 0x01DFEA:'20fc00000000',
 0x01E004:'2486',
 0x01E022:'207c00007fb4',
 0x01E028:'20bc00000001',
 0x01E02E:'23fc00f0300000f02110',
 0x01E038:'23fc0000000100f02114',
 0x01E094:'207c00007fb42210d2bc00000008',
 0x01E0A2:'0cb9000000010004bed4',
 0x01E0B0:'0cb9ffffffff0004bed4',
 0x01E0C8:'741894812202',
 0x01E0CE:'b2b90004bed86d000008',
 0x01E0DE:'b2b90004bedc6e000008',
 0x01E0EE:'e589',
 0x01E0F0:'23fc0004fc400004bee8',
 0x01E0FA:'23f87fa40004beec',
 0x01E13A:'4eb90001e240',
 0x01E140:'207c00007f802010b0bc00000002',
 0x01E14E:'6600ff44',
 0x01E152:'44b90004bed44e75',
 0x01E206:'4eb90001df2c',  # retail Intro page synchronously calls whole CROSS routine
 0x01E20C:'4eb90001e1e2',  # next original page step only after CROSS returns
}

def sample_position(direction:int,phase:int,upper:int,lower:int)->int:
    # Independently spell out the second original CPU loop at $1E094.
    d1=phase+8
    if direction==1: pass
    elif direction==-1:
        d1=24-d1
        if d1>=upper:d1=upper
        if d1<=lower:d1=lower
    else:d1=10
    return 18+4*d1

def run(romfile:pathlib.Path,source:pathlib.Path):
    rom=romfile.read_bytes()
    assert len(rom)==4194304 and hashlib.sha256(rom).hexdigest()==ROM_SHA
    z=zlib.decompressobj(31);cpu=z.decompress(rom[0x3804:]);assert z.eof and len(cpu)==99072
    for site,want in {**CPU_POS,**CPU_GPU,**CHECKS}.items():
        b=bytes.fromhex(want)
        assert cpu[site-BASE:site-BASE+len(b)]==b,f'CPU opcode mismatch at ${site:X}'
    for site in INTRO_CALLS:
        assert cpu[site-BASE:site-BASE+6]==bytes.fromhex('4eb90001e1f2'),hex(site)
    for site,(dest,value) in WRITES.items():
        b=bytes.fromhex('23fc')+(value&0xFFFFFFFF).to_bytes(4,'big')+dest.to_bytes(4,'big')
        assert cpu[site-BASE:site-BASE+10]==b,hex(site)
    # Check signed original 68000 branch destinations, covering boundary between epochs.
    targets=((0x1DFE2,0x1DF2E),(0x1E150,0x1E094),
             (0x1E0AE,0x1E0C4),(0x1E0BC,0x1E0C8),
             (0x1E0C2,0x1E0EE),(0x1E0C6,0x1E0EE))
    for extension,target in targets:
        off=int.from_bytes(cpu[extension-BASE:extension-BASE+2],'big',signed=True)
        assert extension+off==target,(hex(extension),hex(target))
    assert 0x1DFD4<0x1DFDA<0x1DFE4<0x1E038<0x1E094<0x1E0FA<0x1E13A<0x1E140<0x1E152
    gpu=extract(rom);ins,_=disassemble(gpu);verify(gpu,ins)
    for site,(op,a,b,imm) in GPU_INS.items():
        got=ins[site];assert (got[0],got[1],got[2],got[4])==(op,a,b,imm),hex(site)
    assert ins[0xF030C4][:3]==(0x23,1,20) # GPU stage begins at 1
    assert ins[0xF03476][:3]==(0x1F,1,20) # only phase 1 swaps buffer
    assert ins[0xF034F4][:3]==(0x23,2,13) # completion status
    assert len(EXPECTED_KERNELS)==17
    print(f'PASS canonical ROM; {len({**CPU_POS,**CPU_GPU,**CHECKS})} CPU opcode groups, '
          f'{len(GPU_INS)} GPU instruction checkpoints; 6 original branch targets')
    print('PASS epoch A: eight CPU One_Tick calls and OP positions occur BEFORE GPU start')
    print('PASS epoch B: GPU 16-stage job begins only AFTER the eight-tick loop; CPU then publishes $7FA4 bitmap pointer and polls $7F80==2')
    print('PASS stage-1-only GPU buffer exchange; per-stage GPU $7FA8 wait and $7FB4 progress')
    # Original intro caller has explicit direction -1 before the first page
    # and NEG.L after every successful GPU completion.
    direction=-1;upper=10;lower=0
    table=[]
    for i,site in enumerate(INTRO_CALLS):
        if i==1:upper=99
        if i==6:lower=10
        frm=i;to=i+1 if i<6 else 0
        first=tuple(cpu_position(direction,k,upper,lower) for k in range(8))
        later=tuple(sample_position(direction,k,upper,lower) for k in range(1,17))
        assert len(first)==8 and len(later)==16
        # Mode +1 inherits progress+8 in the original $1E0C4 path.
        # Mode -1 applies 24-(progress+8), then upper/lower clamps.
        table.append((frm,to,direction,first,later))
        print(f'PAGE {frm}->{to} direction={direction:+d} '
              f'CPU_PRE_GPU_X={first} GPU_PROGRESS_CPU_X={later}')
        direction=-direction
    assert [r[2] for r in table]==[-1,1,-1,1,-1,1,-1]
    assert table[1][4]==tuple(18+4*(p+8) for p in range(1,17))
    assert table[2][4]==tuple(18+4*(16-p) for p in range(1,17))
    assert table[0][3]==(58,)*8 and table[0][4][0]==58 and table[0][4][-1]==18
    assert table[-1][4]==tuple(max(58,18+4*(16-p)) for p in range(1,17))
    c=source/'src/game/mjp.c'
    h=source/'src/jaguar/avp_gpu/frontend_renderer.c'
    mjp=c.read_text();renderer=h.read_text()
    match=re.search(r'static void mjp_transition\(unsigned from,unsigned to\)\s*\{(.*?)\n\}',mjp,re.S)
    assert match
    fn=match.group(1)
    assert 'mjp_transition(from,to);Clear();if(mjp_text_id(text_id)<0' in mjp
    # R280: MJPP, eight first-epoch VBLs (D5), sixteen GPU-stage VBLs
    # ($7FB4=1..16, D1=$7FB4+8), MJPS, then $01E152 NEG $4BED4.
    i0=fn.index('evt(0x4d4a5050u')
    i1=fn.index('for(s32 d5=0;d5<8;++d5)')
    i2=fn.index('for(s32 stage=1;stage<=16;++stage)')
    i3=fn.index('evt(0x4d4a5053u')
    i4=fn.index('mjp_slide_dir=-mjp_slide_dir;')
    assert i0<i1<i2<i3<i4
    assert 'state->frontend_page_cross_phase+=2u;' not in renderer
    assert 'state->frontend_page_cross_active=0u;' in renderer
    assert 'frontend_blit_scene_cross(fb,from,to,mode,state->frontend_picture_x,' in renderer
    print('PASS original page caller runs the entire CROSS CPU/GPU function before the next page step')
    print('PASS R280 host seam: MJPP -> 8 slide VBLs -> 16 CROSS stage VBLs -> MJPS -> NEG $4BED4')
    print('LIMIT: no CPU/GPU bus-cycle scheduling, VBlank-to-scanout order, image capture or final displayed-frame assertion')

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom',required=True,type=pathlib.Path)
    ap.add_argument('--source',required=True,type=pathlib.Path)
    a=ap.parse_args();run(a.rom,a.source)
