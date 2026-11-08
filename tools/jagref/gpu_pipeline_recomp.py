#!/usr/bin/env python3
"""RE24-S13: static recompilation of the original 3D-view GPU program.

The retail game keeps a small resident sequencer at $F03000-$F0321F of GPU
local RAM.  For every rendered view it copies one overlay after another from
DRAM to $F03220 with a Blitter transfer (resident loader at $F0315C) and
calls it.  The DRAM address of each overlay is read from the longword table
at $7F64,$7F68,$7F6C,$7F70 and its length in longwords is the MOVEI operand
that precedes each call (the code is quoted from the ROM, not assumed):

    F03000 movei #$7f64,r1 / load (r1),r1 / movei #len,r4 / call F0315C ...

Stage k is therefore: the resident code + overlay k at $F03220, entered at
the k-th sequencer block and ending where block k+1 begins (the next load
would overwrite it).  This tool reads the resident image and the DRAM image
captured at the moment the GPU reaches $F03000, decodes the table and emits
one C function per stage, each produced by gpu_recomp.emit() with exactly the
instruction semantics of the TRM JRISC core.

usage: gpu_pipeline_recomp.py gpu_ram.bin dram.bin > pipeline.c
"""
import argparse,json,os,struct,sys
sys.dont_write_bytecode=True
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import gpu_recomp

BASE=0xF03000; RESIDENT_END=0xF03220

# Same FNV-1a byte stream as original_view_native.c. MOVEI immediate bytes
# are excluded ONLY when independently verified 68000 patch-list entries exist.
def fnv64(data):
    h=0xcbf29ce484222325
    for x in data:
        h=((h ^ x)*0x100000001b3)&0xffffffffffffffff
    return h

def be32(b,o):return struct.unpack('>I',b[o:o+4])[0]

def sequencer(gpu):
    """Find the sequencer blocks: movei #table,r1 ; load (r1),r1 ; movei #len,r4"""
    blocks=[];pc=BASE
    while pc<RESIDENT_END-12:
        w=struct.unpack('>H',gpu[pc-BASE:pc-BASE+2])[0]
        if w==0x9801:
            tab=struct.unpack('>H',gpu[pc-BASE+2:pc-BASE+4])[0]|(struct.unpack('>H',gpu[pc-BASE+4:pc-BASE+6])[0]<<16)
            if struct.unpack('>H',gpu[pc-BASE+6:pc-BASE+8])[0]==0xa421 and struct.unpack('>H',gpu[pc-BASE+8:pc-BASE+10])[0]==0x9804:
                ln=struct.unpack('>H',gpu[pc-BASE+10:pc-BASE+12])[0]|(struct.unpack('>H',gpu[pc-BASE+12:pc-BASE+14])[0]<<16)
                blocks.append((pc,tab,ln))
        pc+=2
    return blocks

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gpu',help='same-instant original GPU RAM capture')
    parser.add_argument('dram',help='same-instant original DRAM capture')
    parser.add_argument('--patch-map',help='verified descriptor JSON from gpu_overlay_descriptors.py')
    args=parser.parse_args(argv)
    with open(args.gpu,'rb') as f: gpu=bytearray(f.read())
    with open(args.dram,'rb') as f: dram=f.read()
    blocks=sequencer(gpu)
    patch_rows=[]
    if args.patch_map:
        with open(args.patch_map,encoding='utf8') as f: mapping=json.load(f)
        if mapping.get('format')!='AVP-RE24-S14-verified-overlay-fixups-v1':
            parser.error('unrecognized descriptor map format')
        patch_rows=mapping.get('stages',[])
        if len(patch_rows)!=len(blocks):
            parser.error('descriptor map does not describe every GPU stage')
        if any(row.get('descriptor') is None for row in patch_rows):
            parser.error('descriptor map contains unlocated stages')
    param_records=[]
    # the halt that ends the view: store $F02114 (G_CTRL) after the last block
    out=['/* RE24-S13 GPU 3D-view pipeline, generated from the user\'s ROM by tools/jagref/gpu_pipeline_recomp.py */']
    meta=[]
    for k,(pc,tab,ln) in enumerate(blocks):
        src=be32(dram,tab)&0x1FFFFF
        img=bytearray(gpu)
        if not ln or ln>1024 or src+ln*4>len(dram) or ln*4>len(img)-(RESIDENT_END-BASE):
            parser.error(f'stage {k} has invalid or truncated overlay image')
        ov=dram[src:src+ln*4]
        img[RESIDENT_END-BASE:RESIDENT_END-BASE+len(ov)]=ov
        exits=[b[0] for b in blocks if b[0]!=pc]+[0xF03118]  # $F03118: "jr *" after the G_CTRL write ends the view
        segs=[(BASE,RESIDENT_END),(RESIDENT_END,RESIDENT_END+ln*4)]
        name=f'avp_gpu_stage{k}'
        params={}
        if patch_rows:
            row=patch_rows[k]
            if (row['entry'],row['table'],row['destination'],row['longwords'])!=(pc,tab,src,ln):
                parser.error(f'descriptor mapping for stage {k} does not match current capture')
            for record in row['fixups']:
                target=record['gpu_pc'];variable=record['source_variable']
                off=target-RESIDENT_END
                if not 0<=off<=len(ov)-6 or not 0<=variable<=len(dram)-4:
                    parser.error(f'stage {k} fixup outside captured memory')
                actual=gpu_recomp.dec(img,BASE,target)
                source=be32(dram,variable)
                if (actual[0]!=38 or actual[3]!=source or
                    source!=record['variable_value'] or source!=record['movei_immediate']):
                    parser.error(f'stage {k} GPU ${target:06X} is not a verified patched MOVEI')
                params[target]=len(param_records)
                param_records.append((k,target,variable,source))
                if len(param_records)>256:
                    parser.error('more than 256 descriptor parameters')
        masked=bytearray(ov)
        for target in params:
            off=target-RESIDENT_END
            masked[off+2:off+6]=b'\x00'*4
        stage_hash=fnv64(masked)
        out.append(gpu_recomp.emit(img,BASE,segs,name,exits,
            f'stage {k}: sequencer block ${pc:06X}, table ${tab:04X} -> DRAM ${src:06X}, {ln} longwords at $F03220', movei_params=params))
        meta.append((pc,tab,src,ln,name,stage_hash))
    out.append('static const struct { uint32_t entry,table,src,longs; uint32_t (*fn)(AvpGpuRecompCtx*,uint32_t); } avp_gpu_stages[]={')
    for pc,tab,src,ln,name,h in meta: out.append(f'    {{0x{pc:06x}u,0x{tab:04x}u,0x{src:06x}u,{ln}u,{name}}},')
    out.append('};')
    out.append(f'#define AVP_GPU_STAGE_COUNT {len(meta)}u')
    out.append('/* RE24-S19: byte-exact resident and overlay guards for native execution. */')
    out.append(f'static const uint64_t avp_gpu_resident_hash=UINT64_C(0x{fnv64(gpu[:RESIDENT_END-BASE]):016x});')
    out.append('static const uint64_t avp_gpu_overlay_hashes[]={')
    for pc,tab,src,ln,name,h in meta: out.append(f'    UINT64_C(0x{h:016x}),')
    out.append('};')
    if patch_rows:
        out.append('static const struct { uint32_t stage,gpu_pc,variable,expected; } avp_gpu_params[]={')
        if not param_records:
            out.append('    {0u,0u,0u,0u}, /* empty fallback; count is zero */')
        for k,target,variable,value in param_records:
            out.append(f'    {{{k}u,0x{target:06x}u,0x{variable:06x}u,0x{value:08x}u}},')
        out.append('};')
        out.append(f'#define AVP_GPU_PARAM_COUNT {len(param_records)}u')
    print('\n'.join(out))
    for pc,tab,src,ln,name,h in meta: sys.stderr.write(f'{name}: block {pc:06x} table {tab:04x} src {src:06x} {ln} longs hash {h:016x}\n')

if __name__=='__main__':main()
