#!/usr/bin/env python3
"""RE24-S13: static recompiler for original Jaguar GPU routines.

Input : a GPU local-RAM image (4096 bytes, as captured by jagref with
        JAGM_GPU_SNAP=pc,field) plus the address range of one routine.
Output: a C function that executes that routine with exactly the instruction
        semantics of src/jaguar/hw/jrisc.c (TRM Rev.8 pp.44-58): flags, the
        one-instruction delay slot, register banks (MOVETA/MOVEFA), DIV and
        its remainder, MULT/IMULT, and all memory access through callbacks.
Leaving the range (a jump whose target lies outside it) returns that target.

The generated code is produced from the user's own ROM at build/verification
time; it is not a hand rewrite.  usage:
    gpu_recomp.py gpu_ram.bin LO HI NAME [EXIT,EXIT...] > out.c
EXIT PCs (hex) end the routine before that instruction executes; they are
used where the original code loads its next GPU overlay over this one, after
which the static image no longer describes the program.
"""
import struct,sys

def dec(img,base,pc):
    w=struct.unpack('>H',img[pc-base:pc-base+2])[0]
    op,r1,r2=w>>10,(w>>5)&31,w&31
    imm=None;size=2
    if op==38:
        lo=struct.unpack('>H',img[pc-base+2:pc-base+4])[0];hi=struct.unpack('>H',img[pc-base+4:pc-base+6])[0]
        imm=lo|(hi<<16);size=6
    return op,r1,r2,imm,size

ZN='g->z=(r==0);g->n=(r>>31)&1;'
def stmt(op,r1,r2,imm,pc):
    D=f'R[{r2}]';S=f'R[{r1}]';n=r1 or 32
    if op==0:  return f'{{uint64_t t=(uint64_t){D}+{S};uint32_t r=(uint32_t)t;{ZN}g->c=(t>>32)&1;{D}=r;}}'
    if op==1:  return f'{{uint64_t t=(uint64_t){D}+{S}+g->c;uint32_t r=(uint32_t)t;{ZN}g->c=(t>>32)&1;{D}=r;}}'
    if op==2:  return f'{{uint64_t t=(uint64_t){D}+{n}u;uint32_t r=(uint32_t)t;{ZN}g->c=(t>>32)&1;{D}=r;}}'
    if op==3:  return f'{D}+={n}u;'
    if op==4:  return f'{{uint32_t r={D}-{S};g->c={D}<{S};{ZN}{D}=r;}}'
    if op==5:  return f'{{uint64_t sb=(uint64_t){S}+g->c;uint32_t r=(uint32_t)((uint64_t){D}-sb);g->c=(uint64_t){D}<sb;{ZN}{D}=r;}}'
    if op==6:  return f'{{uint32_t r={D}-{n}u;g->c={D}<{n}u;{ZN}{D}=r;}}'
    if op==7:  return f'{D}-={n}u;'
    if op==8:  return f'{{uint32_t r=0u-{D};g->c={D}!=0;{ZN}{D}=r;}}'
    if op==9:  return f'{{uint32_t r={D}&{S};{ZN}{D}=r;}}'
    if op==10: return f'{{uint32_t r={D}|{S};{ZN}{D}=r;}}'
    if op==11: return f'{{uint32_t r={D}^{S};{ZN}{D}=r;}}'
    if op==12: return f'{{uint32_t r=~{D};{ZN}{D}=r;}}'
    if op==13: return f'g->z=!(({D}>>{r1})&1u);'
    if op==14: return f'{{uint32_t r={D}|(1u<<{r1});{ZN}{D}=r;}}'
    if op==15: return f'{{uint32_t r={D}&~(1u<<{r1});{ZN}{D}=r;}}'
    if op==16: return f'{{uint32_t r=({D}&0xFFFFu)*({S}&0xFFFFu);{ZN}{D}=r;}}'
    if op==17: return f'{{uint32_t r=(uint32_t)((int32_t)(int16_t){D}*(int32_t)(int16_t){S});{ZN}{D}=r;}}'
    if op==21: return f'{D}=jrisc_div_quotient({D},{S},g->divoff,&g->remain);'
    if op==22: return f'{{uint32_t d={D};if(d==0x80000000u){{g->z=0;g->n=1;g->c=1;}}else{{uint32_t r=(d>>31)?0u-d:d;g->c=d>>31;g->z=(r==0);g->n=0;{D}=r;}}}}'
    if op==23 or op==26:
        sra='(uint32_t)((int32_t)d>>(c>=32?31:c))' if op==26 else '(c>=32?0u:(d>>c))'
        return f'{{int32_t c=(int32_t){S};uint32_t d={D},r;if(c>=0){{g->c=d&1u;r={sra};}}else{{g->c=d>>31;c=-c;r=c>=32?0u:(d<<c);}}{ZN}{D}=r;}}'
    if op==24: sh=32-r1; return f'{{uint32_t d={D},r={"0u" if sh>=32 else f"d<<{sh}"};g->c=d>>31;{ZN}{D}=r;}}'
    if op==25: return f'{{uint32_t d={D},r={"0u" if n>=32 else f"d>>{n}"};g->c=d&1u;{ZN}{D}=r;}}'
    if op==27: return f'{{uint32_t d={D},r=(uint32_t)((int32_t)d>>{min(n,31)});g->c=d&1u;{ZN}{D}=r;}}'
    if op==29: k=r1&31; return f'{{uint32_t d={D},r={"d" if k==0 else f"(d>>{k})|(d<<{32-k})"};g->c=d>>31;{ZN}{D}=r;}}'
    if op==30: return f'{{uint32_t r={D}-{S};g->c={D}<{S};{ZN}}}'
    if op==31:
        v=(r1-32) if r1&16 else r1
        return f'{{uint32_t im=(uint32_t){v};uint32_t r={D}-im;g->c={D}<im;{ZN}}}'
    if op==34: return f'{D}={S};'
    if op==35: return f'{D}={r1}u;'
    if op==36: return f'g->r[BANK^1][{r2}]={S};'
    if op==37: return f'{D}=g->r[BANK^1][{r1}];'
    if op==38: return f'{D}=0x{imm:08x}u;'
    # TRM p.36: transfers to the GPU's own local space are always 32-bit, so
    # LOADB/LOADW/STOREB/STOREW there behave as LOAD/STORE (as in jrisc.c).
    if op==39: return f'{{uint32_t a={S}&0xFFFFFFu;{D}=AVP_GPU_LOCAL(a)?g->rd(g->ctx,a,4):(g->rd(g->ctx,a,1)&0xFFu);}}'
    if op==40: return f'{{uint32_t a={S}&0xFFFFFFu;{D}=AVP_GPU_LOCAL(a)?g->rd(g->ctx,a,4):(g->rd(g->ctx,a,2)&0xFFFFu);}}'
    if op==41: return f'{D}=g->rd(g->ctx,{S}&0xFFFFFFu,4);'
    if op==43 or op==44: return f'{D}=g->rd(g->ctx,(R[{14 if op==43 else 15}]+{n*4}u)&0xFFFFFFu,4);'
    if op==45: return f'{{uint32_t a={S}&0xFFFFFFu;if(AVP_GPU_LOCAL(a))g->wr(g->ctx,a,{D},4);else g->wr(g->ctx,a,{D}&0xFFu,1);}}'
    if op==46: return f'{{uint32_t a={S}&0xFFFFFFu;if(AVP_GPU_LOCAL(a))g->wr(g->ctx,a,{D},4);else g->wr(g->ctx,a,{D}&0xFFFFu,2);}}'
    if op==47: return f'g->wr(g->ctx,{S}&0xFFFFFFu,{D},4);'
    if op==49 or op==50: return f'g->wr(g->ctx,(R[{14 if op==49 else 15}]+{n*4}u)&0xFFFFFFu,{D},4);'
    if op==51: return f'{D}=0x{pc:06x}u;'
    if op==57: return ';'
    if op==18: return f'{{int32_t p=(int32_t)(int16_t){D}*(int32_t)(int16_t){S};g->acc=p;g->z=(p==0);g->n=((uint32_t)p>>31)&1;}}'
    if op==20: return f'{{int32_t p=(int32_t)(int16_t){D}*(int32_t)(int16_t){S};g->acc=(int32_t)((uint32_t)g->acc+(uint32_t)p);}}'
    if op==19: return f'{D}=(uint32_t)g->acc;'
    if op==28: return f'{{uint32_t d={D},k={S}&31u,r=k?((d>>k)|(d<<(32u-k))):d;g->c=d>>31;{ZN}{D}=r;}}'
    if op==32: return f'{{int32_t v=(int32_t){D};uint32_t r=v<0?0u:(v>255?255u:(uint32_t)v);g->z=(r==0);g->n=0;{D}=r;}}'
    if op==33: return f'{{int32_t v=(int32_t){D};uint32_t r=v<0?0u:(v>65535?65535u:(uint32_t)v);g->z=(r==0);g->n=0;{D}=r;}}'
    if op==58 or op==59: return f'{D}=g->rd(g->ctx,(R[{14 if op==58 else 15}]+{S})&0xFFFFFFu,4);'
    if op==60 or op==61: return f'g->wr(g->ctx,(R[{14 if op==60 else 15}]+{S})&0xFFFFFFu,{D},4);'
    return None

def cond(cc):
    parts=[]
    if cc&1: parts.append('!g->z')
    if cc&2: parts.append('g->z')
    f='g->n' if cc&16 else 'g->c'
    if cc&4: parts.append(f'!{f}')
    if cc&8: parts.append(f)
    return '&&'.join(parts) or '1'

def emit(img,base,segments,name,exits=(),origin='',movei_params=None):
    """segments: list of (lo,hi) PC ranges decoded linearly from img (based at
    `base`).  A jump to a PC outside every segment returns that PC."""
    ins={}
    for lo,hi in segments:
        pc=lo
        while pc<hi:
            d=dec(img,base,pc);ins[pc]=d;pc+=d[4]
    exits=set(exits)
    movei_params = movei_params or {}
    for target, index in movei_params.items():
        if target not in ins or ins[target][0] != 38 or not isinstance(index, int) or index < 0:
            raise ValueError(f'parameter target ${target:06X} is not a decoded MOVEI')
    def inside(e):return '||'.join(f'(pc>=0x{lo:06x}u&&pc<0x{hi:06x}u)' for lo,hi in segments)
    out=[]
    out.append(f'/* Generated by tools/jagref/gpu_recomp.py {origin}. */')
    out.append('#ifndef AVP_GPU_LOCAL /* GPU local RAM $F03000-$F03FFF and control registers $F02100-$F02123 (jrisc_in_local) */')
    out.append('#define AVP_GPU_LOCAL(a) (((a)>=0xF03000u&&(a)<0xF04000u)||((a)>=0xF02100u&&(a)<0xF02124u))')
    out.append('#endif')
    out.append(f'uint32_t {name}(AvpGpuRecompCtx *g,uint32_t entry)\n{{')
    out.append('    uint32_t *R=g->r[BANK];uint32_t pc=entry,slot_target=0;int take=0;(void)slot_target;(void)take;')
    out.append('  dispatch:\n    R=g->r[BANK];\n    switch(pc){')
    pcs=sorted(ins)
    for p in pcs:
        op,r1,r2,imm,size=ins[p]
        out.append(f'    case 0x{p:06x}u: L_{p:06x}:')
        if p in exits:
            out.append(f'        return 0x{p:06x}u; /* stage exit */');continue
        if op in (52,53):
            tgt=f'R[{r1}]' if op==52 else f'0x{(p+2+2*((r1-32) if r1&16 else r1))&0xFFFFFF:06x}u'
            nxt=p+size
            sop=ins.get(nxt)
            if sop is None or stmt(sop[0],sop[1],sop[2],sop[3],nxt) is None or sop[0] in (52,53,38):
                out.append(f'        g->fault=0x{p:06x}u;return 0xFFFFFFFFu; /* not executable here */');continue
            out.append(f'        take=({cond(r2)});slot_target={tgt};')
            out.append('        '+stmt(sop[0],sop[1],sop[2],sop[3],nxt))
            out.append('        if(take){pc=slot_target;goto dispatch;}')
            out.append(f'        goto L_{nxt+sop[4]:06x};' if nxt+sop[4] in ins else f'        return 0x{nxt+sop[4]:06x}u;')
            continue
        st=(f'R[{r2}]=g->param[{movei_params[p]}];' if p in movei_params
            else stmt(op,r1,r2,imm,p))
        if st is None:
            out.append(f'        g->fault=0x{p:06x}u;return 0xFFFFFFFFu; /* opcode {op}: data or unsupported */');continue
        out.append(f'        if(g->stop)return 0x{p:06x}u;')
        out.append('        '+st)
        if op in (45,46,47,49,50,60,61): out.append('        R=g->r[BANK]; /* a FLAGS store may change the active register bank */')
    last=max(p+ins[p][4] for p in pcs)
    if last not in ins: out.append(f'    case 0x{last:06x}u: return 0x{last:06x}u;')
    out.append('    default: return pc;\n    }\n}')
    return '\n'.join(out)

def main():
    img=open(sys.argv[1],'rb').read();base=0xF03000
    lo,hi=int(sys.argv[2],16),int(sys.argv[3],16);name=sys.argv[4]
    exits=set(int(x,16) for x in sys.argv[5].split(',')) if len(sys.argv)>5 and sys.argv[5] else set()
    print(emit(img,base,[(lo,hi)],name,exits,f'from the retail GPU routine ${lo:06X}-${hi:06X}'))

if __name__=='__main__':main()
